#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include "TopologyManager.h"
#include <functional>
#include <map>

namespace P2POverlay {

/**
 * Handles different types of messages in the P2P overlay
 */
class MessageHandler {
public:
    MessageHandler(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager,
        std::shared_ptr<TopologyManager> topologyManager
    );
    ~MessageHandler();
    
    // Message processing
    void processMessage(const Message& message);
    void handleJoinRequest(const Message& message);
    void handleJoinResponse(const Message& message);
    void handleLeaveNotification(const Message& message);
    void handleHeartbeat(const Message& message);
    void handleDataMessage(const Message& message);
    void handleTopologyUpdate(const Message& message);
    void handlePeerDiscovery(const Message& message);
    
    // Message creation
    Message createJoinRequest(NodeID targetNodeID);
    Message createJoinResponse(NodeID targetNodeID, bool accepted, const std::vector<NodeID>& peerList);
    Message createLeaveNotification(NodeID targetNodeID);
    Message createHeartbeat(NodeID targetNodeID);
    Message createDataMessage(NodeID targetNodeID, const std::vector<uint8_t>& data);
    Message createTopologyUpdate(const std::vector<NodeID>& updatedNodes);
    Message createPeerDiscoveryRequest(NodeID targetNodeID, int maxPeers);
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<TopologyManager> topologyManager_;
    
    // Message type handlers map
    std::map<MessageType, std::function<void(const Message&)>> messageHandlers_;
    
    // Helper methods
    uint64_t getCurrentTimestamp() const;
    std::vector<uint8_t> serializeNodeList(const std::vector<NodeID>& nodes);
    std::vector<NodeID> deserializeNodeList(const std::vector<uint8_t>& data);
};

} // namespace P2POverlay

#endif // MESSAGE_HANDLER_H

