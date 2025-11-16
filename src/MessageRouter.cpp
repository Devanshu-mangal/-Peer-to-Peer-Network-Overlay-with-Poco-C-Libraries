#include "MessageRouter.h"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace P2POverlay {

MessageRouter::MessageRouter(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager,
    std::shared_ptr<TopologyManager> topologyManager)
    : node_(node), networkManager_(networkManager), topologyManager_(topologyManager),
      routedMessageCount_(0), forwardedMessageCount_(0), totalHopCount_(0) {
}

MessageRouter::~MessageRouter() {
    clearRoutingTable();
}

bool MessageRouter::routeMessage(const Message& message, RoutingStrategy strategy) {
    routedMessageCount_++;
    
    switch (strategy) {
        case RoutingStrategy::DIRECT:
            return routeMessageDirect(message.receiverID, message);
        case RoutingStrategy::SHORTEST_PATH:
            return routeMessageMultiHop(message.receiverID, message);
        case RoutingStrategy::FLOOD:
            return floodMessage(message);
        default:
            return routeMessageMultiHop(message.receiverID, message);
    }
}

bool MessageRouter::routeMessageDirect(NodeID targetID, const Message& message) {
    // Check if directly connected
    if (node_->hasPeer(targetID)) {
        return networkManager_->sendMessageToPeer(targetID, message);
    }
    
    // Fall back to multi-hop
    return routeMessageMultiHop(targetID, message);
}

bool MessageRouter::routeMessageMultiHop(NodeID targetID, const Message& message) {
    std::vector<NodeID> route = findRoute(targetID);
    
    if (route.empty()) {
        std::cerr << "No route found to node " << targetID << std::endl;
        return false;
    }
    
    if (route.size() == 1) {
        // Direct connection
        return networkManager_->sendMessageToPeer(targetID, message);
    }
    
    // Multi-hop: send to next hop
    NodeID nextHop = route[1];
    totalHopCount_ += route.size() - 1;
    
    // Create routed message with path information
    Message routedMsg = message;
    // In a real implementation, we'd encode the route in the message
    
    return networkManager_->sendMessageToPeer(nextHop, routedMsg);
}

bool MessageRouter::floodMessage(const Message& message, int /*maxHops*/) {
    uint64_t msgID = generateMessageID(message);
    
    // Check if we've seen this message
    if (isMessageSeen(msgID)) {
        return false; // Already processed
    }
    
    markMessageSeen(msgID);
    
    // Broadcast to all peers except sender
    std::vector<NodeID> peers = node_->getPeerIDs();
    bool success = true;
    
    for (NodeID peerID : peers) {
        if (peerID != message.senderID) {
            if (!networkManager_->sendMessageToPeer(peerID, message)) {
                success = false;
            }
        }
    }
    
    forwardedMessageCount_++;
    return success;
}

std::vector<NodeID> MessageRouter::findRoute(NodeID targetID) const {
    // Check if directly connected
    if (node_->hasPeer(targetID)) {
        return {node_->getID(), targetID};
    }
    
    // Use topology manager to find path
    return topologyManager_->findPath(node_->getID(), targetID);
}

int MessageRouter::getHopCount(NodeID targetID) const {
    std::vector<NodeID> route = findRoute(targetID);
    if (route.empty()) {
        return -1; // Unreachable
    }
    return static_cast<int>(route.size()) - 1;
}

bool MessageRouter::isReachable(NodeID targetID) const {
    return !findRoute(targetID).empty();
}

bool MessageRouter::forwardMessage(const Message& message, const RoutingInfo& routingInfo) {
    if (routingInfo.hopCount <= 0) {
        return false; // Reached destination or invalid
    }
    
    // Check if we're the destination
    if (message.receiverID == node_->getID()) {
        return true; // Message delivered
    }
    
    // Find next hop
    std::vector<NodeID> route = findRoute(message.receiverID);
    if (route.size() < 2) {
        return false;
    }
    
    NodeID nextHop = route[1];
    forwardedMessageCount_++;
    
    return networkManager_->sendMessageToPeer(nextHop, message);
}

void MessageRouter::handleIncomingRoute(const Message& message, const RoutingInfo& routingInfo) {
    // Process routed message
    if (message.receiverID == node_->getID()) {
        // We're the destination
        return;
    }
    
    // Forward to next hop
    forwardMessage(message, routingInfo);
}

void MessageRouter::updateRoutingTable() {
    std::lock_guard<std::mutex> lock(routingTableMutex_);
    
    // Update routing table based on topology
    std::vector<NodeID> allNodes = topologyManager_->getAllNodeIDs();
    NodeID selfID = node_->getID();
    
    routingTable_.clear();
    hopCounts_.clear();
    
    for (NodeID targetID : allNodes) {
        if (targetID == selfID) {
            continue;
        }
        
        std::vector<NodeID> path = topologyManager_->findPath(selfID, targetID);
        if (!path.empty() && path.size() > 1) {
            routingTable_[targetID] = path[1]; // Next hop
            hopCounts_[targetID] = static_cast<int>(path.size()) - 1;
            routeTimestamps_[targetID] = std::chrono::system_clock::now();
        }
    }
}

void MessageRouter::clearRoutingTable() {
    std::lock_guard<std::mutex> lock(routingTableMutex_);
    routingTable_.clear();
    hopCounts_.clear();
    routeTimestamps_.clear();
}

std::map<NodeID, std::vector<NodeID>> MessageRouter::getRoutingTable() const {
    std::lock_guard<std::mutex> lock(routingTableMutex_);
    std::map<NodeID, std::vector<NodeID>> table;
    
    for (const auto& pair : routingTable_) {
        std::vector<NodeID> path = topologyManager_->findPath(node_->getID(), pair.first);
        table[pair.first] = path;
    }
    
    return table;
}

double MessageRouter::getAverageHopCount() const {
    if (routedMessageCount_ == 0) {
        return 0.0;
    }
    return static_cast<double>(totalHopCount_) / routedMessageCount_;
}

std::vector<NodeID> MessageRouter::computeShortestPath(NodeID targetID) const {
    return topologyManager_->findPath(node_->getID(), targetID);
}

NodeID MessageRouter::getNextHop(NodeID targetID) const {
    std::lock_guard<std::mutex> lock(routingTableMutex_);
    auto it = routingTable_.find(targetID);
    if (it != routingTable_.end()) {
        return it->second;
    }
    
    // Compute on the fly
    std::vector<NodeID> path = computeShortestPath(targetID);
    if (path.size() > 1) {
        return path[1];
    }
    
    return 0;
}

bool MessageRouter::isMessageSeen(uint64_t messageID) const {
    std::lock_guard<std::mutex> lock(seenMessagesMutex_);
    return seenMessages_.find(messageID) != seenMessages_.end();
}

void MessageRouter::markMessageSeen(uint64_t messageID) {
    std::lock_guard<std::mutex> lock(seenMessagesMutex_);
    seenMessages_[messageID] = std::chrono::system_clock::now();
}

void MessageRouter::cleanupSeenMessages(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(seenMessagesMutex_);
    auto now = std::chrono::system_clock::now();
    
    auto it = seenMessages_.begin();
    while (it != seenMessages_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second);
        if (elapsed.count() > timeoutSeconds) {
            it = seenMessages_.erase(it);
        } else {
            ++it;
        }
    }
}

uint64_t MessageRouter::generateMessageID(const Message& message) const {
    // Simple hash-based ID generation
    uint64_t id = message.senderID;
    id ^= message.receiverID;
    id ^= message.timestamp;
    return id;
}

} // namespace P2POverlay

