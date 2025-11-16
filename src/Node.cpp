#include "Node.h"
#include <algorithm>
#include <chrono>

namespace P2POverlay {

Node::Node(NodeID id, const NetworkAddress& address)
    : nodeID_(id), address_(address), isActive_(true) {
    lastSeen_ = std::chrono::system_clock::now();
}

Node::~Node() {
    setActive(false);
}

bool Node::addPeer(NodeID peerID, const NetworkAddress& peerAddress) {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    // Check if peer already exists
    auto it = std::find(peerIDs_.begin(), peerIDs_.end(), peerID);
    if (it != peerIDs_.end()) {
        return false;
    }
    
    // Check peer limit
    if (peerIDs_.size() >= MAX_PEERS) {
        return false;
    }
    
    peerIDs_.push_back(peerID);
    peerAddresses_.push_back(peerAddress);
    return true;
}

bool Node::removePeer(NodeID peerID) {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    auto it = std::find(peerIDs_.begin(), peerIDs_.end(), peerID);
    if (it == peerIDs_.end()) {
        return false;
    }
    
    size_t index = std::distance(peerIDs_.begin(), it);
    peerIDs_.erase(it);
    peerAddresses_.erase(peerAddresses_.begin() + index);
    return true;
}

std::vector<NodeID> Node::getPeerIDs() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peerIDs_;
}

std::vector<NetworkAddress> Node::getPeerAddresses() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peerAddresses_;
}

bool Node::hasPeer(NodeID peerID) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return std::find(peerIDs_.begin(), peerIDs_.end(), peerID) != peerIDs_.end();
}

size_t Node::getPeerCount() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peerIDs_.size();
}

void Node::updateLastSeen() {
    std::lock_guard<std::mutex> lock(heartbeatMutex_);
    lastSeen_ = std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point Node::getLastSeen() const {
    std::lock_guard<std::mutex> lock(heartbeatMutex_);
    return lastSeen_;
}

bool Node::isAlive(int timeoutSeconds) const {
    std::lock_guard<std::mutex> lock(heartbeatMutex_);
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastSeen_);
    return elapsed.count() < timeoutSeconds;
}

bool Node::sendMessage(const Message& /*message*/) {
    // Message sending is handled by NetworkManager
    return true;
}

bool Node::receiveMessage(Message& /*message*/) {
    // Message receiving is handled by NetworkManager
    return true;
}

void Node::setTopologyInfo(const std::vector<NodeID>& neighbors) {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    topologyNeighbors_ = neighbors;
}

std::vector<NodeID> Node::getTopologyInfo() const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    return topologyNeighbors_;
}

} // namespace P2POverlay

