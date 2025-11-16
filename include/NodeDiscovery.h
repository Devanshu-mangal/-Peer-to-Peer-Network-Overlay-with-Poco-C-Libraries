#ifndef NODE_DISCOVERY_H
#define NODE_DISCOVERY_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include "TopologyManager.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>

namespace P2POverlay {

/**
 * Handles node discovery mechanisms for the P2P overlay
 */
class NodeDiscovery {
public:
    NodeDiscovery(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager,
        std::shared_ptr<TopologyManager> topologyManager
    );
    ~NodeDiscovery();
    
    // Bootstrap and initial discovery
    bool discoverNetwork(const std::vector<NetworkAddress>& bootstrapNodes);
    bool connectToBootstrapNode(const NetworkAddress& bootstrapAddress);
    
    // Peer discovery
    std::vector<NodeID> discoverPeers(int maxPeers = MAX_PEERS);
    bool requestPeerList(NodeID fromNode, int maxPeers = MAX_PEERS);
    
    // Active discovery
    void startPeriodicDiscovery(int intervalSeconds = 60);
    void stopPeriodicDiscovery();
    bool isDiscoveryActive() const { return discoveryActive_; }
    
    // Discovery callbacks
    void setOnPeerDiscoveredCallback(std::function<void(NodeID, const NetworkAddress&)> callback);
    void setOnDiscoveryFailedCallback(std::function<void(const NetworkAddress&)> callback);
    
    // Discovery statistics
    size_t getDiscoveredNodeCount() const;
    std::vector<NodeID> getDiscoveredNodes() const;
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<TopologyManager> topologyManager_;
    
    // Discovery state
    std::atomic<bool> discoveryActive_;
    mutable std::mutex discoveredNodesMutex_;
    std::map<NodeID, NetworkAddress> discoveredNodes_;
    std::map<NodeID, std::chrono::system_clock::time_point> discoveryTimestamps_;
    
    // Callbacks
    std::function<void(NodeID, const NetworkAddress&)> onPeerDiscovered_;
    std::function<void(const NetworkAddress&)> onDiscoveryFailed_;
    
    // Internal methods
    bool validateNodeAddress(const NetworkAddress& address) const;
    void addDiscoveredNode(NodeID nodeID, const NetworkAddress& address);
    void removeStaleNodes(int timeoutSeconds = 300);
};

} // namespace P2POverlay

#endif // NODE_DISCOVERY_H

