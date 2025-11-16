#ifndef RELIABLE_MESSAGING_H
#define RELIABLE_MESSAGING_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>

namespace P2POverlay {

/**
 * Message acknowledgment
 */
enum class AckStatus {
    PENDING,
    ACKNOWLEDGED,
    TIMEOUT,
    FAILED
};

/**
 * Reliable message information
 */
struct ReliableMessage {
    uint64_t messageID;
    Message message;
    NodeID destinationID;
    AckStatus ackStatus;
    int retryCount;
    std::chrono::system_clock::time_point sendTime;
    std::chrono::system_clock::time_point lastRetry;
    
    ReliableMessage() : messageID(0), destinationID(0), 
                        ackStatus(AckStatus::PENDING), retryCount(0) {}
};

/**
 * Provides reliable message delivery with acknowledgments
 */
class ReliableMessaging {
public:
    ReliableMessaging(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager
    );
    ~ReliableMessaging();
    
    // Reliable sending
    uint64_t sendReliableMessage(NodeID targetID, const Message& message);
    bool acknowledgeMessage(uint64_t messageID, NodeID senderID);
    
    // Message tracking
    bool isMessageAcknowledged(uint64_t messageID) const;
    void retryPendingMessages(int timeoutSeconds = 30, int maxRetries = 3);
    void cleanupAcknowledgedMessages(int timeoutSeconds = 300);
    
    // Configuration
    void setRetryTimeout(int timeoutSeconds) { retryTimeout_ = timeoutSeconds; }
    void setMaxRetries(int maxRetries) { maxRetries_ = maxRetries; }
    
    // Callbacks
    void setOnMessageDeliveredCallback(std::function<void(uint64_t, NodeID)> callback);
    void setOnMessageFailedCallback(std::function<void(uint64_t, NodeID)> callback);
    
    // Statistics
    size_t getSentMessages() const { return sentMessages_; }
    size_t getAcknowledgedMessages() const { return acknowledgedMessages_; }
    size_t getFailedMessages() const { return failedMessages_; }
    double getDeliveryRate() const;
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    
    // Pending messages waiting for acknowledgment
    mutable std::mutex pendingMessagesMutex_;
    std::map<uint64_t, ReliableMessage> pendingMessages_;
    
    // Configuration
    int retryTimeout_;
    int maxRetries_;
    
    // Callbacks
    std::function<void(uint64_t, NodeID)> onMessageDelivered_;
    std::function<void(uint64_t, NodeID)> onMessageFailed_;
    
    // Statistics
    std::atomic<size_t> sentMessages_;
    std::atomic<size_t> acknowledgedMessages_;
    std::atomic<size_t> failedMessages_;
    
    // Internal methods
    uint64_t generateMessageID();
    bool sendWithRetry(const ReliableMessage& reliableMsg);
    void markMessageAcknowledged(uint64_t messageID);
    void markMessageFailed(uint64_t messageID);
};

} // namespace P2POverlay

#endif // RELIABLE_MESSAGING_H

