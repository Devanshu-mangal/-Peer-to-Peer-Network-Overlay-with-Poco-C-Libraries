#include "DataExchange.h"
#include "MessageHandler.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstring>

namespace P2POverlay {

DataExchange::DataExchange(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager,
    std::shared_ptr<MessageRouter> messageRouter)
    : node_(node), networkManager_(networkManager), messageRouter_(messageRouter),
      chunkSize_(4096), maxConcurrentTransfers_(5),
      sentDataSize_(0), receivedDataSize_(0), completedTransfers_(0), failedTransfers_(0) {
}

DataExchange::~DataExchange() {
    cleanupCompletedTransfers(0);
}

uint64_t DataExchange::sendData(NodeID targetID, const std::vector<uint8_t>& data, const std::string& dataType) {
    uint64_t transferID = generateTransferID();
    
    // Create transfer record
    DataTransfer transfer;
    transfer.transferID = transferID;
    transfer.sourceID = node_->getID();
    transfer.destinationID = targetID;
    transfer.dataType = dataType;
    transfer.totalSize = data.size();
    transfer.transferredSize = 0;
    transfer.status = TransferStatus::IN_PROGRESS;
    transfer.startTime = std::chrono::system_clock::now();
    transfer.lastUpdate = transfer.startTime;
    
    {
        std::lock_guard<std::mutex> lock(transfersMutex_);
        outgoingTransfers_[transferID] = transfer;
    }
    
    // Split data into chunks
    std::vector<DataChunk> chunks = splitData(data, transferID);
    
    // Send chunks
    for (const DataChunk& chunk : chunks) {
        if (!sendDataChunk(targetID, chunk)) {
            markTransferComplete(transferID, false);
            return 0;
        }
        
        updateTransferProgress(transferID, chunk.data.size());
    }
    
    markTransferComplete(transferID, true);
    return transferID;
}

bool DataExchange::sendDataChunk(NodeID targetID, const DataChunk& chunk) {
    // Create message with chunk data
    Message msg;
    msg.type = MessageType::DATA_CHUNK;
    msg.senderID = node_->getID();
    msg.receiverID = targetID;
    msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // Serialize chunk (simplified)
    msg.payload.resize(sizeof(DataChunk) + chunk.data.size());
    std::memcpy(msg.payload.data(), &chunk, sizeof(DataChunk));
    std::memcpy(msg.payload.data() + sizeof(DataChunk), chunk.data.data(), chunk.data.size());
    
    sentDataSize_ += chunk.data.size();
    
    // Route message
    return messageRouter_->routeMessage(msg, RoutingStrategy::SHORTEST_PATH);
}

bool DataExchange::cancelTransfer(uint64_t transferID) {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    
    auto it = outgoingTransfers_.find(transferID);
    if (it != outgoingTransfers_.end()) {
        it->second.status = TransferStatus::CANCELLED;
        return true;
    }
    
    return false;
}

void DataExchange::handleDataChunk(const DataChunk& chunk, NodeID sourceID) {
    uint64_t transferID = chunk.chunkID;
    
    {
        std::lock_guard<std::mutex> lock(receivedDataMutex_);
        receivedChunks_[transferID][chunk.sequenceNumber] = chunk;
    }
    
    // Update transfer info
    {
        std::lock_guard<std::mutex> lock(transfersMutex_);
        auto it = incomingTransfers_.find(transferID);
        if (it == incomingTransfers_.end()) {
            DataTransfer transfer;
            transfer.transferID = transferID;
            transfer.sourceID = sourceID;
            transfer.destinationID = node_->getID();
            transfer.status = TransferStatus::IN_PROGRESS;
            transfer.startTime = std::chrono::system_clock::now();
            incomingTransfers_[transferID] = transfer;
        }
        
        auto& transfer = incomingTransfers_[transferID];
        transfer.transferredSize += chunk.data.size();
        transfer.lastUpdate = std::chrono::system_clock::now();
        
        if (chunk.isLastChunk) {
            transfer.totalSize = transfer.transferredSize;
        }
    }
    
    receivedDataSize_ += chunk.data.size();
    
    // Check if all chunks received
    if (reassembleData(transferID)) {
        markTransferComplete(transferID, true);
    }
    
    // Notify progress
    if (onTransferProgress_) {
        std::lock_guard<std::mutex> lock(transfersMutex_);
        auto it = incomingTransfers_.find(transferID);
        if (it != incomingTransfers_.end()) {
            onTransferProgress_(transferID, it->second.transferredSize, it->second.totalSize);
        }
    }
}

std::vector<uint8_t> DataExchange::getReceivedData(uint64_t transferID) const {
    std::lock_guard<std::mutex> lock(receivedDataMutex_);
    auto it = completedData_.find(transferID);
    if (it != completedData_.end()) {
        return it->second;
    }
    return {};
}

bool DataExchange::isTransferComplete(uint64_t transferID) const {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    auto it = incomingTransfers_.find(transferID);
    if (it != incomingTransfers_.end()) {
        return it->second.status == TransferStatus::COMPLETED;
    }
    return false;
}

std::vector<DataTransfer> DataExchange::getActiveTransfers() const {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    std::vector<DataTransfer> transfers;
    
    for (const auto& pair : outgoingTransfers_) {
        if (pair.second.status == TransferStatus::IN_PROGRESS) {
            transfers.push_back(pair.second);
        }
    }
    
    for (const auto& pair : incomingTransfers_) {
        if (pair.second.status == TransferStatus::IN_PROGRESS) {
            transfers.push_back(pair.second);
        }
    }
    
    return transfers;
}

DataTransfer DataExchange::getTransferInfo(uint64_t transferID) const {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    
    auto it = outgoingTransfers_.find(transferID);
    if (it != outgoingTransfers_.end()) {
        return it->second;
    }
    
    it = incomingTransfers_.find(transferID);
    if (it != incomingTransfers_.end()) {
        return it->second;
    }
    
    return DataTransfer();
}

void DataExchange::cleanupCompletedTransfers(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    auto now = std::chrono::system_clock::now();
    
    // Cleanup outgoing transfers
    auto outIt = outgoingTransfers_.begin();
    while (outIt != outgoingTransfers_.end()) {
        if (outIt->second.status == TransferStatus::COMPLETED ||
            outIt->second.status == TransferStatus::FAILED ||
            outIt->second.status == TransferStatus::CANCELLED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - outIt->second.lastUpdate
            );
            if (elapsed.count() > timeoutSeconds) {
                outIt = outgoingTransfers_.erase(outIt);
            } else {
                ++outIt;
            }
        } else {
            ++outIt;
        }
    }
    
    // Cleanup incoming transfers
    auto inIt = incomingTransfers_.begin();
    while (inIt != incomingTransfers_.end()) {
        if (inIt->second.status == TransferStatus::COMPLETED ||
            inIt->second.status == TransferStatus::FAILED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - inIt->second.lastUpdate
            );
            if (elapsed.count() > timeoutSeconds) {
                inIt = incomingTransfers_.erase(inIt);
            } else {
                ++inIt;
            }
        } else {
            ++inIt;
        }
    }
}

void DataExchange::setOnDataReceivedCallback(
    std::function<void(NodeID, const std::vector<uint8_t>&, const std::string&)> callback) {
    onDataReceived_ = callback;
}

void DataExchange::setOnTransferCompleteCallback(std::function<void(uint64_t, bool)> callback) {
    onTransferComplete_ = callback;
}

void DataExchange::setOnTransferProgressCallback(
    std::function<void(uint64_t, size_t, size_t)> callback) {
    onTransferProgress_ = callback;
}

uint64_t DataExchange::generateTransferID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

std::vector<DataChunk> DataExchange::splitData(const std::vector<uint8_t>& data, uint64_t transferID) {
    std::vector<DataChunk> chunks;
    size_t totalSize = data.size();
    uint32_t chunkCount = static_cast<uint32_t>((totalSize + chunkSize_ - 1) / chunkSize_);
    
    for (uint32_t i = 0; i < chunkCount; ++i) {
        DataChunk chunk;
        chunk.chunkID = transferID;
        chunk.sequenceNumber = i;
        chunk.totalChunks = chunkCount;
        chunk.isLastChunk = (i == chunkCount - 1);
        
        size_t offset = i * chunkSize_;
        size_t chunkDataSize = std::min(chunkSize_, totalSize - offset);
        chunk.data.resize(chunkDataSize);
        std::memcpy(chunk.data.data(), data.data() + offset, chunkDataSize);
        
        chunks.push_back(chunk);
    }
    
    return chunks;
}

bool DataExchange::reassembleData(uint64_t transferID) {
    std::lock_guard<std::mutex> lock(receivedDataMutex_);
    
    auto chunksIt = receivedChunks_.find(transferID);
    if (chunksIt == receivedChunks_.end()) {
        return false;
    }
    
    const auto& chunks = chunksIt->second;
    if (chunks.empty()) {
        return false;
    }
    
    // Check if we have all chunks
    uint32_t expectedChunks = chunks.begin()->second.totalChunks;
    if (chunks.size() < expectedChunks) {
        return false; // Not all chunks received yet
    }
    
    // Reassemble data
    std::vector<uint8_t> reassembled;
    for (uint32_t i = 0; i < expectedChunks; ++i) {
        auto it = chunks.find(i);
        if (it == chunks.end()) {
            return false; // Missing chunk
        }
        reassembled.insert(reassembled.end(), it->second.data.begin(), it->second.data.end());
    }
    
    completedData_[transferID] = reassembled;
    
    // Notify callback (release lock first)
    std::string dataType;
    NodeID sourceID = 0;
    {
        std::lock_guard<std::mutex> transferLock(transfersMutex_);
        auto transferIt = incomingTransfers_.find(transferID);
        if (transferIt != incomingTransfers_.end()) {
            sourceID = transferIt->second.sourceID;
            dataType = transferIt->second.dataType;
        }
    }
    
    if (onDataReceived_ && sourceID != 0) {
        onDataReceived_(sourceID, reassembled, dataType);
    }
    
    return true;
}

void DataExchange::updateTransferProgress(uint64_t transferID, size_t bytesTransferred) {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    
    auto it = outgoingTransfers_.find(transferID);
    if (it != outgoingTransfers_.end()) {
        it->second.transferredSize += bytesTransferred;
        it->second.lastUpdate = std::chrono::system_clock::now();
        
        if (onTransferProgress_) {
            onTransferProgress_(transferID, it->second.transferredSize, it->second.totalSize);
        }
    }
}

void DataExchange::markTransferComplete(uint64_t transferID, bool success) {
    std::lock_guard<std::mutex> lock(transfersMutex_);
    
    auto it = outgoingTransfers_.find(transferID);
    if (it != outgoingTransfers_.end()) {
        it->second.status = success ? TransferStatus::COMPLETED : TransferStatus::FAILED;
        it->second.lastUpdate = std::chrono::system_clock::now();
        
        if (success) {
            completedTransfers_++;
        } else {
            failedTransfers_++;
        }
        
        if (onTransferComplete_) {
            onTransferComplete_(transferID, success);
        }
    }
    
    it = incomingTransfers_.find(transferID);
    if (it != incomingTransfers_.end()) {
        it->second.status = success ? TransferStatus::COMPLETED : TransferStatus::FAILED;
        it->second.lastUpdate = std::chrono::system_clock::now();
    }
}

} // namespace P2POverlay

