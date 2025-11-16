#include "ReliableMessaging.h"
#include "MessageHandler.h"
#include <iostream>
#include <random>
#include <chrono>

namespace P2POverlay {

ReliableMessaging::ReliableMessaging(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager)
    : node_(node), networkManager_(networkManager),
      retryTimeout_(30), maxRetries_(3),
      sentMessages_(0), acknowledgedMessages_(0), failedMessages_(0) {
}

ReliableMessaging::~ReliableMessaging() {
    cleanupAcknowledgedMessages(0);
}

uint64_t ReliableMessaging::sendReliableMessage(NodeID targetID, const Message& message) {
    uint64_t messageID = generateMessageID();
    
    ReliableMessage reliableMsg;
    reliableMsg.messageID = messageID;
    reliableMsg.message = message;
    reliableMsg.destinationID = targetID;
    reliableMsg.ackStatus = AckStatus::PENDING;
    reliableMsg.retryCount = 0;
    reliableMsg.sendTime = std::chrono::system_clock::now();
    reliableMsg.lastRetry = reliableMsg.sendTime;
    
    // Add message ID to payload (in real implementation)
    // For now, we'll track it separately
    
    {
        std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
        pendingMessages_[messageID] = reliableMsg;
    }
    
    // Send message
    if (networkManager_->sendMessageToPeer(targetID, message)) {
        sentMessages_++;
        return messageID;
    } else {
        markMessageFailed(messageID);
        return 0;
    }
}

bool ReliableMessaging::acknowledgeMessage(uint64_t messageID, NodeID senderID) {
    std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
    
    auto it = pendingMessages_.find(messageID);
    if (it != pendingMessages_.end()) {
        it->second.ackStatus = AckStatus::ACKNOWLEDGED;
        acknowledgedMessages_++;
        
        if (onMessageDelivered_) {
            onMessageDelivered_(messageID, senderID);
        }
        
        return true;
    }
    
    return false;
}

bool ReliableMessaging::isMessageAcknowledged(uint64_t messageID) const {
    std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
    auto it = pendingMessages_.find(messageID);
    if (it != pendingMessages_.end()) {
        return it->second.ackStatus == AckStatus::ACKNOWLEDGED;
    }
    return false;
}

void ReliableMessaging::retryPendingMessages(int timeoutSeconds, int maxRetries) {
    std::vector<uint64_t> toRetry;
    std::vector<uint64_t> toFail;
    
    {
        std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
        auto now = std::chrono::system_clock::now();
        
        for (auto& pair : pendingMessages_) {
            ReliableMessage& msg = pair.second;
            
            if (msg.ackStatus == AckStatus::ACKNOWLEDGED) {
                continue;
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - msg.lastRetry);
            
            if (elapsed.count() >= timeoutSeconds) {
                if (msg.retryCount < maxRetries) {
                    toRetry.push_back(pair.first);
                } else {
                    toFail.push_back(pair.first);
                }
            }
        }
    }
    
    // Retry messages
    for (uint64_t msgID : toRetry) {
        ReliableMessage msg;
        {
            std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
            auto it = pendingMessages_.find(msgID);
            if (it != pendingMessages_.end()) {
                msg = it->second;
                it->second.retryCount++;
                it->second.lastRetry = std::chrono::system_clock::now();
            } else {
                continue;
            }
        }
        
        sendWithRetry(msg);
    }
    
    // Mark failed messages
    for (uint64_t msgID : toFail) {
        markMessageFailed(msgID);
    }
}

void ReliableMessaging::cleanupAcknowledgedMessages(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
    auto now = std::chrono::system_clock::now();
    
    auto it = pendingMessages_.begin();
    while (it != pendingMessages_.end()) {
        if (it->second.ackStatus == AckStatus::ACKNOWLEDGED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.sendTime
            );
            
            if (elapsed.count() > timeoutSeconds) {
                it = pendingMessages_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

void ReliableMessaging::setOnMessageDeliveredCallback(std::function<void(uint64_t, NodeID)> callback) {
    onMessageDelivered_ = callback;
}

void ReliableMessaging::setOnMessageFailedCallback(std::function<void(uint64_t, NodeID)> callback) {
    onMessageFailed_ = callback;
}

double ReliableMessaging::getDeliveryRate() const {
    if (sentMessages_ == 0) {
        return 0.0;
    }
    return static_cast<double>(acknowledgedMessages_) / sentMessages_ * 100.0;
}

uint64_t ReliableMessaging::generateMessageID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

bool ReliableMessaging::sendWithRetry(const ReliableMessage& reliableMsg) {
    return networkManager_->sendMessageToPeer(reliableMsg.destinationID, reliableMsg.message);
}

void ReliableMessaging::markMessageAcknowledged(uint64_t messageID) {
    acknowledgeMessage(messageID, 0);
}

void ReliableMessaging::markMessageFailed(uint64_t messageID) {
    std::lock_guard<std::mutex> lock(pendingMessagesMutex_);
    
    auto it = pendingMessages_.find(messageID);
    if (it != pendingMessages_.end()) {
        it->second.ackStatus = AckStatus::FAILED;
        failedMessages_++;
        
        if (onMessageFailed_) {
            onMessageFailed_(messageID, it->second.destinationID);
        }
        
        pendingMessages_.erase(it);
    }
}

} // namespace P2POverlay

