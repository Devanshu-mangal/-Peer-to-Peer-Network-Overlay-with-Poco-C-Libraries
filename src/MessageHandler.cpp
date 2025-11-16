#include "MessageHandler.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>

namespace P2POverlay {

MessageHandler::MessageHandler(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager,
    std::shared_ptr<TopologyManager> topologyManager)
    : node_(node), networkManager_(networkManager), topologyManager_(topologyManager) {
    
    // Register message handlers
    messageHandlers_[MessageType::JOIN_REQUEST] = [this](const Message& msg) { handleJoinRequest(msg); };
    messageHandlers_[MessageType::JOIN_RESPONSE] = [this](const Message& msg) { handleJoinResponse(msg); };
    messageHandlers_[MessageType::LEAVE_NOTIFICATION] = [this](const Message& msg) { handleLeaveNotification(msg); };
    messageHandlers_[MessageType::HEARTBEAT] = [this](const Message& msg) { handleHeartbeat(msg); };
    messageHandlers_[MessageType::DATA_MESSAGE] = [this](const Message& msg) { handleDataMessage(msg); };
    messageHandlers_[MessageType::TOPOLOGY_UPDATE] = [this](const Message& msg) { handleTopologyUpdate(msg); };
    messageHandlers_[MessageType::PEER_DISCOVERY] = [this](const Message& msg) { handlePeerDiscovery(msg); };
}

MessageHandler::~MessageHandler() {
}

void MessageHandler::processMessage(const Message& message) {
    auto it = messageHandlers_.find(message.type);
    if (it != messageHandlers_.end()) {
        it->second(message);
    } else {
        std::cerr << "Unknown message type: " << static_cast<int>(message.type) << std::endl;
    }
}

void MessageHandler::handleJoinRequest(const Message& message) {
    std::cout << "Received JOIN_REQUEST from node " << message.senderID << std::endl;
    
    // Check if we can accept the new peer
    bool accepted = node_->getPeerCount() < MAX_PEERS;
    
    std::vector<NodeID> peerList;
    if (accepted) {
        // Get list of potential peers to suggest
        peerList = topologyManager_->discoverPeers(message.senderID, MAX_PEERS);
        
        // Add the new peer to our local node
        // Note: We need the peer's address, which should be in the message payload
        // For now, we'll assume it's handled elsewhere
    }
    
    // Send join response
    Message response = createJoinResponse(message.senderID, accepted, peerList);
    networkManager_->sendMessageToPeer(message.senderID, response);
}

void MessageHandler::handleJoinResponse(const Message& message) {
    std::cout << "Received JOIN_RESPONSE from node " << message.senderID << std::endl;
    
    // Parse peer list from payload
    std::vector<NodeID> peerList = deserializeNodeList(message.payload);
    
    // Connect to suggested peers
    for (NodeID peerID : peerList) {
        if (peerID != node_->getID() && !node_->hasPeer(peerID)) {
            NetworkAddress peerAddr = topologyManager_->getNodeAddress(peerID);
            if (peerAddr.port != 0) {
                networkManager_->connectToPeer(peerAddr);
            }
        }
    }
}

void MessageHandler::handleLeaveNotification(const Message& message) {
    std::cout << "Received LEAVE_NOTIFICATION from node " << message.senderID << std::endl;
    
    // Remove peer from local node
    node_->removePeer(message.senderID);
    
    // Remove from topology
    topologyManager_->removeNode(message.senderID);
    
    // Broadcast topology update
    Message update = createTopologyUpdate({message.senderID});
    networkManager_->broadcastMessage(update, message.senderID);
}

void MessageHandler::handleHeartbeat(const Message& message) {
    // Update last seen time for the peer
    // This is handled at a higher level, but we acknowledge it here
    node_->updateLastSeen();
    
    // Send heartbeat response
    Message heartbeat = createHeartbeat(message.senderID);
    networkManager_->sendMessageToPeer(message.senderID, heartbeat);
}

void MessageHandler::handleDataMessage(const Message& message) {
    std::cout << "Received DATA_MESSAGE from node " << message.senderID 
              << " (size: " << message.payload.size() << " bytes)" << std::endl;
    
    // Process data message
    // In a real implementation, this would route the message or process it
}

void MessageHandler::handleTopologyUpdate(const Message& message) {
    std::cout << "Received TOPOLOGY_UPDATE from node " << message.senderID << std::endl;
    
    // Parse updated nodes from payload
    std::vector<NodeID> updatedNodes = deserializeNodeList(message.payload);
    
    // Update local topology information
    for (NodeID nodeID : updatedNodes) {
        if (!topologyManager_->nodeExists(nodeID)) {
            // Node was removed, update our view
            node_->removePeer(nodeID);
        }
    }
    
    // Validate and repair topology if needed
    topologyManager_->checkNetworkIntegrity();
}

void MessageHandler::handlePeerDiscovery(const Message& message) {
    std::cout << "Received PEER_DISCOVERY from node " << message.senderID << std::endl;
    
    // Extract max peers from payload (first 4 bytes)
    int maxPeers = MAX_PEERS;
    if (message.payload.size() >= sizeof(int)) {
        std::memcpy(&maxPeers, message.payload.data(), sizeof(int));
    }
    
    // Discover peers and send response
    std::vector<NodeID> peers = topologyManager_->discoverPeers(message.senderID, maxPeers);
    Message response = createJoinResponse(message.senderID, true, peers);
    networkManager_->sendMessageToPeer(message.senderID, response);
}

Message MessageHandler::createJoinRequest(NodeID targetNodeID) {
    Message msg;
    msg.type = MessageType::JOIN_REQUEST;
    msg.senderID = node_->getID();
    msg.receiverID = targetNodeID;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message MessageHandler::createJoinResponse(NodeID targetNodeID, bool accepted, const std::vector<NodeID>& peerList) {
    Message msg;
    msg.type = MessageType::JOIN_RESPONSE;
    msg.senderID = node_->getID();
    msg.receiverID = targetNodeID;
    msg.timestamp = getCurrentTimestamp();
    msg.payload = serializeNodeList(peerList);
    
    // Add acceptance flag at the beginning
    uint8_t acceptFlag = accepted ? 1 : 0;
    msg.payload.insert(msg.payload.begin(), acceptFlag);
    
    return msg;
}

Message MessageHandler::createLeaveNotification(NodeID targetNodeID) {
    Message msg;
    msg.type = MessageType::LEAVE_NOTIFICATION;
    msg.senderID = node_->getID();
    msg.receiverID = targetNodeID;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message MessageHandler::createHeartbeat(NodeID targetNodeID) {
    Message msg;
    msg.type = MessageType::HEARTBEAT;
    msg.senderID = node_->getID();
    msg.receiverID = targetNodeID;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message MessageHandler::createDataMessage(NodeID targetNodeID, const std::vector<uint8_t>& data) {
    Message msg;
    msg.type = MessageType::DATA_MESSAGE;
    msg.senderID = node_->getID();
    msg.receiverID = targetNodeID;
    msg.timestamp = getCurrentTimestamp();
    msg.payload = data;
    return msg;
}

Message MessageHandler::createTopologyUpdate(const std::vector<NodeID>& updatedNodes) {
    Message msg;
    msg.type = MessageType::TOPOLOGY_UPDATE;
    msg.senderID = node_->getID();
    msg.receiverID = 0; // Broadcast
    msg.timestamp = getCurrentTimestamp();
    msg.payload = serializeNodeList(updatedNodes);
    return msg;
}

Message MessageHandler::createPeerDiscoveryRequest(NodeID targetNodeID, int maxPeers) {
    Message msg;
    msg.type = MessageType::PEER_DISCOVERY;
    msg.senderID = node_->getID();
    msg.receiverID = targetNodeID;
    msg.timestamp = getCurrentTimestamp();
    msg.payload.resize(sizeof(int));
    std::memcpy(msg.payload.data(), &maxPeers, sizeof(int));
    return msg;
}

uint64_t MessageHandler::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::vector<uint8_t> MessageHandler::serializeNodeList(const std::vector<NodeID>& nodes) {
    std::vector<uint8_t> data;
    uint32_t count = static_cast<uint32_t>(nodes.size());
    
    // Write count
    data.resize(sizeof(uint32_t) + count * sizeof(NodeID));
    std::memcpy(data.data(), &count, sizeof(uint32_t));
    
    // Write node IDs
    if (!nodes.empty()) {
        std::memcpy(data.data() + sizeof(uint32_t), nodes.data(), count * sizeof(NodeID));
    }
    
    return data;
}

std::vector<NodeID> MessageHandler::deserializeNodeList(const std::vector<uint8_t>& data) {
    std::vector<NodeID> nodes;
    
    if (data.size() < sizeof(uint32_t)) {
        return nodes;
    }
    
    uint32_t count = 0;
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    
    size_t expectedSize = sizeof(uint32_t) + count * sizeof(NodeID);
    if (data.size() < expectedSize) {
        return nodes;
    }
    
    nodes.resize(count);
    if (count > 0) {
        std::memcpy(nodes.data(), data.data() + sizeof(uint32_t), count * sizeof(NodeID));
    }
    
    return nodes;
}

} // namespace P2POverlay

