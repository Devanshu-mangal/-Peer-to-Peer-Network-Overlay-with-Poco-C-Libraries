#include "DynamicNodeManager.h"
#include "MessageHandler.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

namespace P2POverlay {

DynamicNodeManager::DynamicNodeManager(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager,
    std::shared_ptr<TopologyManager> topologyManager)
    : node_(node), networkManager_(networkManager), topologyManager_(topologyManager),
      failureDetectionActive_(false) {
}

DynamicNodeManager::~DynamicNodeManager() {
    stopFailureDetection();
}

bool DynamicNodeManager::addNode(NodeID nodeID, const NetworkAddress& address) {
    if (!validateNodeAddition(nodeID, address)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(nodesMutex_);
    
    // Check if node already exists
    if (nodeRegistry_.find(nodeID) != nodeRegistry_.end()) {
        return false;
    }
    
    // Create node info
    NodeInfo info;
    info.nodeID = nodeID;
    info.address = address;
    info.state = NodeState::JOINING;
    info.joinTime = std::chrono::system_clock::now();
    info.lastSeen = info.joinTime;
    info.failureCount = 0;
    
    // Add to registry
    nodeRegistry_[nodeID] = info;
    
    // Add to topology
    if (!topologyManager_->addNode(nodeID, address)) {
        nodeRegistry_.erase(nodeID);
        return false;
    }
    
    // Add as peer if we have capacity
    if (node_->getPeerCount() < MAX_PEERS) {
        node_->addPeer(nodeID, address);
        networkManager_->connectToPeer(address);
    }
    
    // Update state
    info.state = NodeState::ACTIVE;
    nodeRegistry_[nodeID] = info;
    
    std::cout << "Added node " << nodeID << " at " << address.toString() << std::endl;
    
    if (onNodeAdded_) {
        onNodeAdded_(nodeID, address);
    }
    
    // Propagate topology update
    propagateTopologyUpdate({nodeID});
    
    return true;
}

bool DynamicNodeManager::addNodeWithValidation(NodeID nodeID, const NetworkAddress& address) {
    // Additional validation before adding
    if (nodeID == node_->getID()) {
        return false; // Cannot add self
    }
    
    if (topologyManager_->nodeExists(nodeID)) {
        return false; // Node already exists
    }
    
    return addNode(nodeID, address);
}

std::vector<NodeID> DynamicNodeManager::addNodesFromList(
    const std::vector<std::pair<NodeID, NetworkAddress>>& nodes) {
    std::vector<NodeID> addedNodes;
    
    for (const auto& pair : nodes) {
        if (addNode(pair.first, pair.second)) {
            addedNodes.push_back(pair.first);
        }
    }
    
    return addedNodes;
}

bool DynamicNodeManager::removeNode(NodeID nodeID, bool graceful) {
    if (graceful) {
        return removeNodeGracefully(nodeID);
    } else {
        return removeNodeForced(nodeID);
    }
}

bool DynamicNodeManager::removeNodeGracefully(NodeID nodeID) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    
    auto it = nodeRegistry_.find(nodeID);
    if (it == nodeRegistry_.end()) {
        return false;
    }
    
    // Update state
    it->second.state = NodeState::LEAVING;
    
    // Send leave notification (would use MessageHandler)
    // For now, we'll just remove
    
    // Remove from peer list
    node_->removePeer(nodeID);
    
    // Disconnect
    networkManager_->disconnectFromPeer(nodeID);
    
    // Remove from topology
    topologyManager_->removeNode(nodeID);
    
    // Remove from registry
    nodeRegistry_.erase(it);
    
    std::cout << "Gracefully removed node " << nodeID << std::endl;
    
    if (onNodeRemoved_) {
        onNodeRemoved_(nodeID);
    }
    
    // Repair network if needed
    repairNetworkAfterNodeRemoval(nodeID);
    
    return true;
}

bool DynamicNodeManager::removeNodeForced(NodeID nodeID) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    
    auto it = nodeRegistry_.find(nodeID);
    if (it == nodeRegistry_.end()) {
        return false;
    }
    
    // Mark as failed
    it->second.state = NodeState::FAILED;
    
    // Remove from peer list
    node_->removePeer(nodeID);
    
    // Disconnect
    networkManager_->disconnectFromPeer(nodeID);
    
    // Remove from topology
    topologyManager_->removeNode(nodeID);
    
    // Remove from registry
    nodeRegistry_.erase(it);
    
    std::cout << "Forced removal of node " << nodeID << std::endl;
    
    if (onNodeFailed_) {
        onNodeFailed_(nodeID);
    }
    
    // Repair network
    repairNetworkAfterNodeRemoval(nodeID);
    
    return true;
}

NodeState DynamicNodeManager::getNodeState(NodeID nodeID) const {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        return it->second.state;
    }
    return NodeState::UNKNOWN;
}

void DynamicNodeManager::setNodeState(NodeID nodeID, NodeState state) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        it->second.state = state;
    }
}

std::vector<NodeID> DynamicNodeManager::getNodesByState(NodeState state) const {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    std::vector<NodeID> nodes;
    
    for (const auto& pair : nodeRegistry_) {
        if (pair.second.state == state) {
            nodes.push_back(pair.first);
        }
    }
    
    return nodes;
}

void DynamicNodeManager::detectFailedNodes(int timeoutSeconds) {
    std::vector<NodeID> failedNodes;
    
    {
        std::lock_guard<std::mutex> lock(nodesMutex_);
        auto now = std::chrono::system_clock::now();
        
        for (auto& pair : nodeRegistry_) {
            if (pair.second.state != NodeState::ACTIVE) {
                continue;
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - pair.second.lastSeen
            );
            
            if (elapsed.count() > timeoutSeconds) {
                incrementFailureCount(pair.first);
                
                if (shouldRemoveNode(pair.first)) {
                    failedNodes.push_back(pair.first);
                }
            } else {
                resetFailureCount(pair.first);
            }
        }
    }
    
    // Remove failed nodes (lock is released after scope)
    for (NodeID nodeID : failedNodes) {
        removeNodeForced(nodeID);
    }
}

std::vector<NodeID> DynamicNodeManager::getFailedNodes() const {
    return getNodesByState(NodeState::FAILED);
}

bool DynamicNodeManager::recoverFromNodeFailure(NodeID failedNodeID) {
    std::cout << "Attempting to recover from failure of node " << failedNodeID << std::endl;
    
    // Find replacement connections
    std::vector<NodeID> replacements = findReplacementConnections(failedNodeID);
    
    if (replacements.empty()) {
        std::cout << "No replacement connections found" << std::endl;
        return false;
    }
    
    // Establish replacement connections
    if (establishReplacementConnections(replacements)) {
        std::cout << "Successfully recovered from node failure" << std::endl;
        return true;
    }
    
    return false;
}

void DynamicNodeManager::startFailureDetection(int intervalSeconds) {
    if (failureDetectionActive_) {
        return;
    }
    
    failureDetectionActive_ = true;
    // Failure detection started silently
    
    // In a real implementation, this would run in a separate thread
}

void DynamicNodeManager::stopFailureDetection() {
    failureDetectionActive_ = false;
    std::cout << "Failure detection stopped" << std::endl;
}

bool DynamicNodeManager::maintainNetworkIntegrity() {
    // Validate topology
    topologyManager_->validateTopology();
    
    // Check connectivity
    if (!topologyManager_->isTopologyConnected()) {
        std::cout << "Network is disconnected, attempting repair..." << std::endl;
        topologyManager_->repairTopology();
    }
    
    // Detect failed nodes
    detectFailedNodes(NODE_TIMEOUT_SEC);
    
    // Ensure connectivity
    return ensureConnectivity();
}

bool DynamicNodeManager::repairNetworkAfterNodeRemoval(NodeID removedNodeID) {
    std::cout << "Repairing network after removal of node " << removedNodeID << std::endl;
    
    // Check if network is still connected
    if (!topologyManager_->isTopologyConnected()) {
        std::cout << "Network disconnected, repairing topology..." << std::endl;
        topologyManager_->repairTopology();
    }
    
    // Find replacement connections if needed
    std::vector<NodeID> replacements = findReplacementConnections(removedNodeID);
    if (!replacements.empty()) {
        establishReplacementConnections(replacements);
    }
    
    // Validate integrity
    bool integrityOK = topologyManager_->checkNetworkIntegrity();
    
    if (integrityOK && onNetworkRepaired_) {
        onNetworkRepaired_();
    }
    
    return integrityOK;
}

bool DynamicNodeManager::ensureConnectivity() {
    // Check if we have enough peers
    if (node_->getPeerCount() < MAX_PEERS) {
        // Try to discover and connect to more peers
        std::vector<NodeID> allNodes = topologyManager_->getAllNodeIDs();
        NodeID selfID = node_->getID();
        
        for (NodeID nodeID : allNodes) {
            if (nodeID == selfID) continue;
            if (node_->hasPeer(nodeID)) continue;
            if (node_->getPeerCount() >= MAX_PEERS) break;
            
            NetworkAddress addr = topologyManager_->getNodeAddress(nodeID);
            if (addr.port != 0) {
                node_->addPeer(nodeID, addr);
                networkManager_->connectToPeer(addr);
            }
        }
    }
    
    return topologyManager_->isTopologyConnected();
}

void DynamicNodeManager::propagateTopologyUpdate(const std::vector<NodeID>& /*updatedNodes*/) {
    // Broadcast topology update to all peers
    // This would use MessageHandler in real implementation
    std::vector<NodeID> peers = node_->getPeerIDs();
    
    // Send topology update messages to all peers
    // (Implementation would use MessageHandler)
    (void)peers; // Suppress unused variable warning
}

void DynamicNodeManager::handleTopologyChange(NodeID changedNodeID, bool added) {
    if (added) {
        setNodeState(changedNodeID, NodeState::ACTIVE);
    } else {
        setNodeState(changedNodeID, NodeState::FAILED);
    }
}

size_t DynamicNodeManager::getActiveNodeCount() const {
    return getNodesByState(NodeState::ACTIVE).size();
}

size_t DynamicNodeManager::getFailedNodeCount() const {
    return getNodesByState(NodeState::FAILED).size();
}

std::vector<NodeInfo> DynamicNodeManager::getAllNodeInfo() const {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    std::vector<NodeInfo> infoList;
    
    for (const auto& pair : nodeRegistry_) {
        infoList.push_back(pair.second);
    }
    
    return infoList;
}

NodeInfo DynamicNodeManager::getNodeInfo(NodeID nodeID) const {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        return it->second;
    }
    return NodeInfo();
}

void DynamicNodeManager::setOnNodeAddedCallback(std::function<void(NodeID, const NetworkAddress&)> callback) {
    onNodeAdded_ = callback;
}

void DynamicNodeManager::setOnNodeRemovedCallback(std::function<void(NodeID)> callback) {
    onNodeRemoved_ = callback;
}

void DynamicNodeManager::setOnNodeFailedCallback(std::function<void(NodeID)> callback) {
    onNodeFailed_ = callback;
}

void DynamicNodeManager::setOnNetworkRepairedCallback(std::function<void()> callback) {
    onNetworkRepaired_ = callback;
}

bool DynamicNodeManager::validateNodeAddition(NodeID nodeID, const NetworkAddress& address) const {
    if (nodeID == 0 || address.port == 0 || address.host.empty()) {
        return false;
    }
    
    if (nodeID == node_->getID()) {
        return false;
    }
    
    return true;
}

void DynamicNodeManager::updateNodeLastSeen(NodeID nodeID) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        it->second.lastSeen = std::chrono::system_clock::now();
        resetFailureCount(nodeID);
    }
}

void DynamicNodeManager::incrementFailureCount(NodeID nodeID) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        it->second.failureCount++;
    }
}

void DynamicNodeManager::resetFailureCount(NodeID nodeID) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        it->second.failureCount = 0;
    }
}

bool DynamicNodeManager::shouldRemoveNode(NodeID nodeID) const {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        // Remove if failure count exceeds threshold
        return it->second.failureCount >= 3;
    }
    return false;
}

void DynamicNodeManager::notifyNodeRemoval(NodeID nodeID) {
    if (onNodeRemoved_) {
        onNodeRemoved_(nodeID);
    }
}

std::vector<NodeID> DynamicNodeManager::findReplacementConnections(NodeID removedNodeID) const {
    std::vector<NodeID> replacements;
    
    // Get all nodes except self and removed node
    std::vector<NodeID> allNodes = topologyManager_->getAllNodeIDs();
    NodeID selfID = node_->getID();
    
    for (NodeID nodeID : allNodes) {
        if (nodeID == selfID || nodeID == removedNodeID) {
            continue;
        }
        
        if (!node_->hasPeer(nodeID) && node_->getPeerCount() < MAX_PEERS) {
            replacements.push_back(nodeID);
        }
    }
    
    return replacements;
}

bool DynamicNodeManager::establishReplacementConnections(const std::vector<NodeID>& replacements) {
    bool success = false;
    
    for (NodeID nodeID : replacements) {
        if (node_->getPeerCount() >= MAX_PEERS) {
            break;
        }
        
        NetworkAddress addr = topologyManager_->getNodeAddress(nodeID);
        if (addr.port != 0) {
            if (node_->addPeer(nodeID, addr)) {
                if (networkManager_->connectToPeer(addr)) {
                    success = true;
                }
            }
        }
    }
    
    return success;
}

} // namespace P2POverlay

