#ifndef DATA_EXCHANGE_H
#define DATA_EXCHANGE_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include "MessageRouter.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <string>

namespace P2POverlay {

/**
 * Data chunk information
 */
struct DataChunk {
    uint64_t chunkID;
    uint32_t sequenceNumber;
    uint32_t totalChunks;
    std::vector<uint8_t> data;
    bool isLastChunk;
    
    DataChunk() : chunkID(0), sequenceNumber(0), totalChunks(0), isLastChunk(false) {}
};

/**
 * Data transfer status
 */
enum class TransferStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    CANCELLED
};

/**
 * Data transfer information
 */
struct DataTransfer {
    uint64_t transferID;
    NodeID sourceID;
    NodeID destinationID;
    std::string dataType;
    size_t totalSize;
    size_t transferredSize;
    TransferStatus status;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastUpdate;
    
    DataTransfer() : transferID(0), sourceID(0), destinationID(0), 
                     totalSize(0), transferredSize(0), status(TransferStatus::PENDING) {}
};

/**
 * Handles data exchange between nodes
 */
class DataExchange {
public:
    DataExchange(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager,
        std::shared_ptr<MessageRouter> messageRouter
    );
    ~DataExchange();
    
    // Data sending
    uint64_t sendData(NodeID targetID, const std::vector<uint8_t>& data, const std::string& dataType = "generic");
    bool sendDataChunk(NodeID targetID, const DataChunk& chunk);
    bool cancelTransfer(uint64_t transferID);
    
    // Data receiving
    void handleDataChunk(const DataChunk& chunk, NodeID sourceID);
    std::vector<uint8_t> getReceivedData(uint64_t transferID) const;
    bool isTransferComplete(uint64_t transferID) const;
    
    // Transfer management
    std::vector<DataTransfer> getActiveTransfers() const;
    DataTransfer getTransferInfo(uint64_t transferID) const;
    void cleanupCompletedTransfers(int timeoutSeconds = 3600);
    
    // Callbacks
    void setOnDataReceivedCallback(std::function<void(NodeID, const std::vector<uint8_t>&, const std::string&)> callback);
    void setOnTransferCompleteCallback(std::function<void(uint64_t, bool)> callback);
    void setOnTransferProgressCallback(std::function<void(uint64_t, size_t, size_t)> callback);
    
    // Configuration
    void setChunkSize(size_t chunkSize) { chunkSize_ = chunkSize; }
    size_t getChunkSize() const { return chunkSize_; }
    void setMaxConcurrentTransfers(size_t maxTransfers) { maxConcurrentTransfers_ = maxTransfers; }
    
    // Statistics
    size_t getSentDataSize() const { return sentDataSize_; }
    size_t getReceivedDataSize() const { return receivedDataSize_; }
    size_t getCompletedTransfers() const { return completedTransfers_; }
    size_t getFailedTransfers() const { return failedTransfers_; }
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<MessageRouter> messageRouter_;
    
    // Transfer management
    mutable std::mutex transfersMutex_;
    std::map<uint64_t, DataTransfer> outgoingTransfers_;
    std::map<uint64_t, DataTransfer> incomingTransfers_;
    
    // Received data buffers
    mutable std::mutex receivedDataMutex_;
    std::map<uint64_t, std::map<uint32_t, DataChunk>> receivedChunks_;
    std::map<uint64_t, std::vector<uint8_t>> completedData_;
    
    // Configuration
    size_t chunkSize_;
    size_t maxConcurrentTransfers_;
    
    // Callbacks
    std::function<void(NodeID, const std::vector<uint8_t>&, const std::string&)> onDataReceived_;
    std::function<void(uint64_t, bool)> onTransferComplete_;
    std::function<void(uint64_t, size_t, size_t)> onTransferProgress_;
    
    // Statistics
    std::atomic<size_t> sentDataSize_;
    std::atomic<size_t> receivedDataSize_;
    std::atomic<size_t> completedTransfers_;
    std::atomic<size_t> failedTransfers_;
    
    // Internal methods
    uint64_t generateTransferID();
    std::vector<DataChunk> splitData(const std::vector<uint8_t>& data, uint64_t transferID);
    bool reassembleData(uint64_t transferID);
    void updateTransferProgress(uint64_t transferID, size_t bytesTransferred);
    void markTransferComplete(uint64_t transferID, bool success);
};

} // namespace P2POverlay

#endif // DATA_EXCHANGE_H

