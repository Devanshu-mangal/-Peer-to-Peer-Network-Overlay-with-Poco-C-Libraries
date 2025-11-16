#ifndef NODE_H
#define NODE_H

#include "Common.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>

namespace P2POverlay {

/**
 * Represents a peer node in the P2P overlay network
 */
class Node {
public:
    Node(NodeID id, const NetworkAddress& address);
    ~Node();
    
    // Node identification
    NodeID getID() const { return nodeID_; }
    NetworkAddress getAddress() const { return address_; }
    
    // Peer management
    bool addPeer(NodeID peerID, const NetworkAddress& peerAddress);
    bool removePeer(NodeID peerID);
    std::vector<NodeID> getPeerIDs() const;
    std::vector<NetworkAddress> getPeerAddresses() const;
    bool hasPeer(NodeID peerID) const;
    size_t getPeerCount() const;
    
    // Node state
    bool isActive() const { return isActive_; }
    void setActive(bool active) { isActive_ = active; }
    
    // Heartbeat management
    void updateLastSeen();
    std::chrono::system_clock::time_point getLastSeen() const;
    bool isAlive(int timeoutSeconds = NODE_TIMEOUT_SEC) const;
    
    // Network operations
    bool sendMessage(const Message& message);
    bool receiveMessage(Message& message);
    
    // Topology information
    void setTopologyInfo(const std::vector<NodeID>& neighbors);
    std::vector<NodeID> getTopologyInfo() const;
    
private:
    NodeID nodeID_;
    NetworkAddress address_;
    std::atomic<bool> isActive_;
    
    // Peer connections
    mutable std::mutex peersMutex_;
    std::vector<NodeID> peerIDs_;
    std::vector<NetworkAddress> peerAddresses_;
    
    // Heartbeat tracking
    mutable std::mutex heartbeatMutex_;
    std::chrono::system_clock::time_point lastSeen_;
    
    // Topology information
    mutable std::mutex topologyMutex_;
    std::vector<NodeID> topologyNeighbors_;
};

} // namespace P2POverlay

#endif // NODE_H

