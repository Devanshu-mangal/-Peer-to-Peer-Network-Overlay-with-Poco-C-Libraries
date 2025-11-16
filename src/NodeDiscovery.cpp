#include "NodeDiscovery.h"
#include "MessageHandler.h"
#include <Poco/Net/SocketAddress.h>
#include <Poco/Exception.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>

namespace P2POverlay {

NodeDiscovery::NodeDiscovery(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager,
    std::shared_ptr<TopologyManager> topologyManager)
    : node_(node), networkManager_(networkManager), topologyManager_(topologyManager),
      discoveryActive_(false) {
}

NodeDiscovery::~NodeDiscovery() {
    stopPeriodicDiscovery();
}

bool NodeDiscovery::discoverNetwork(const std::vector<NetworkAddress>& bootstrapNodes) {
    if (bootstrapNodes.empty()) {
        std::cout << "No bootstrap nodes provided for discovery" << std::endl;
        return false;
    }
    
    std::cout << "Starting network discovery with " << bootstrapNodes.size() << " bootstrap node(s)" << std::endl;
    
    // Try to connect to bootstrap nodes
    bool connected = false;
    for (const auto& bootstrapAddr : bootstrapNodes) {
        if (connectToBootstrapNode(bootstrapAddr)) {
            connected = true;
            std::cout << "Successfully connected to bootstrap node: " << bootstrapAddr.toString() << std::endl;
            break;
        }
    }
    
    if (!connected) {
        std::cerr << "Failed to connect to any bootstrap node" << std::endl;
        if (onDiscoveryFailed_) {
            for (const auto& addr : bootstrapNodes) {
                onDiscoveryFailed_(addr);
            }
        }
        return false;
    }
    
    // Discover peers through bootstrap node
    std::vector<NodeID> peers = discoverPeers(MAX_PEERS);
    std::cout << "Discovered " << peers.size() << " peer(s)" << std::endl;
    
    return true;
}

bool NodeDiscovery::connectToBootstrapNode(const NetworkAddress& bootstrapAddress) {
    if (!validateNodeAddress(bootstrapAddress)) {
        return false;
    }
    
    try {
        if (networkManager_->connectToPeer(bootstrapAddress)) {
            // Request peer list from bootstrap node
            // Note: In a real implementation, we'd need to know the bootstrap node's ID
            // For now, we'll use a simplified approach
            return true;
        }
    } catch (Poco::Exception& e) {
        std::cerr << "Failed to connect to bootstrap node " << bootstrapAddress.toString() 
                  << ": " << e.displayText() << std::endl;
    }
    
    return false;
}

std::vector<NodeID> NodeDiscovery::discoverPeers(int maxPeers) {
    std::vector<NodeID> discovered;
    
    // Get known nodes from topology
    std::vector<NodeID> knownNodes = topologyManager_->getAllNodeIDs();
    
    // Filter out self and already connected peers
    NodeID selfID = node_->getID();
    std::vector<NodeID> currentPeers = node_->getPeerIDs();
    
    for (NodeID nodeID : knownNodes) {
        if (nodeID == selfID) {
            continue;
        }
        
        if (std::find(currentPeers.begin(), currentPeers.end(), nodeID) != currentPeers.end()) {
            continue; // Already connected
        }
        
        NetworkAddress addr = topologyManager_->getNodeAddress(nodeID);
        if (addr.port != 0) {
            addDiscoveredNode(nodeID, addr);
            discovered.push_back(nodeID);
            
            if (onPeerDiscovered_) {
                onPeerDiscovered_(nodeID, addr);
            }
            
            if (static_cast<int>(discovered.size()) >= maxPeers) {
                break;
            }
        }
    }
    
    return discovered;
}

bool NodeDiscovery::requestPeerList(NodeID fromNode, int /*maxPeers*/) {
    // This would send a PEER_DISCOVERY message
    // Implementation depends on MessageHandler integration
    NetworkAddress addr = topologyManager_->getNodeAddress(fromNode);
    if (addr.port == 0) {
        return false;
    }
    
    // Connect if not already connected
    if (!networkManager_->isConnectedTo(fromNode)) {
        if (!networkManager_->connectToPeer(addr)) {
            return false;
        }
    }
    
    // Request would be sent via MessageHandler
    return true;
}

void NodeDiscovery::startPeriodicDiscovery(int /*intervalSeconds*/) {
    if (discoveryActive_) {
        return;
    }
    
    discoveryActive_ = true;
    
    // In a real implementation, this would run in a separate thread
    // For now, we'll mark it as active
    // Periodic discovery started silently
}

void NodeDiscovery::stopPeriodicDiscovery() {
    discoveryActive_ = false;
    // Periodic discovery stopped silently
}

void NodeDiscovery::setOnPeerDiscoveredCallback(std::function<void(NodeID, const NetworkAddress&)> callback) {
    onPeerDiscovered_ = callback;
}

void NodeDiscovery::setOnDiscoveryFailedCallback(std::function<void(const NetworkAddress&)> callback) {
    onDiscoveryFailed_ = callback;
}

size_t NodeDiscovery::getDiscoveredNodeCount() const {
    std::lock_guard<std::mutex> lock(discoveredNodesMutex_);
    return discoveredNodes_.size();
}

std::vector<NodeID> NodeDiscovery::getDiscoveredNodes() const {
    std::lock_guard<std::mutex> lock(discoveredNodesMutex_);
    std::vector<NodeID> nodes;
    for (const auto& pair : discoveredNodes_) {
        nodes.push_back(pair.first);
    }
    return nodes;
}

bool NodeDiscovery::validateNodeAddress(const NetworkAddress& address) const {
    if (address.host.empty() || address.port == 0) {
        return false;
    }
    
    // Basic validation - check if it's not our own address
    NetworkAddress selfAddr = node_->getAddress();
    if (address == selfAddr) {
        return false;
    }
    
    return true;
}

void NodeDiscovery::addDiscoveredNode(NodeID nodeID, const NetworkAddress& address) {
    std::lock_guard<std::mutex> lock(discoveredNodesMutex_);
    discoveredNodes_[nodeID] = address;
    discoveryTimestamps_[nodeID] = std::chrono::system_clock::now();
}

void NodeDiscovery::removeStaleNodes(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(discoveredNodesMutex_);
    auto now = std::chrono::system_clock::now();
    
    auto it = discoveryTimestamps_.begin();
    while (it != discoveryTimestamps_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second);
        if (elapsed.count() > timeoutSeconds) {
            discoveredNodes_.erase(it->first);
            it = discoveryTimestamps_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace P2POverlay

