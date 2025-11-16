#ifndef TOPOLOGY_MANAGER_H
#define TOPOLOGY_MANAGER_H

#include "Common.h"
#include "Node.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <set>

namespace P2POverlay {

/**
 * Manages the overlay network topology
 */
class TopologyManager {
public:
    TopologyManager(std::shared_ptr<Node> node);
    ~TopologyManager();
    
    // Topology operations
    bool addNode(NodeID nodeID, const NetworkAddress& address);
    bool removeNode(NodeID nodeID);
    bool updateNodeAddress(NodeID nodeID, const NetworkAddress& newAddress);
    
    // Node discovery
    std::vector<NodeID> discoverPeers(NodeID requestingNodeID, int maxPeers = MAX_PEERS);
    bool registerNode(NodeID nodeID, const NetworkAddress& address);
    
    // Topology queries
    bool nodeExists(NodeID nodeID) const;
    NetworkAddress getNodeAddress(NodeID nodeID) const;
    std::vector<NodeID> getAllNodeIDs() const;
    size_t getNetworkSize() const;
    
    // Routing and path finding
    std::vector<NodeID> findPath(NodeID from, NodeID to) const;
    std::vector<NodeID> getNeighbors(NodeID nodeID) const;
    
    // Topology maintenance
    void updateTopology();
    void validateTopology();
    bool isTopologyConnected() const;
    
    // Network integrity
    bool checkNetworkIntegrity();
    void repairTopology();
    
    // Bootstrap node management
    void addBootstrapNode(const NetworkAddress& address);
    std::vector<NetworkAddress> getBootstrapNodes() const;
    
private:
    std::shared_ptr<Node> localNode_;
    
    // Network topology graph
    mutable std::mutex topologyMutex_;
    std::map<NodeID, NetworkAddress> nodeRegistry_;
    std::map<NodeID, std::set<NodeID>> adjacencyList_;
    
    // Bootstrap nodes for initial network entry
    std::vector<NetworkAddress> bootstrapNodes_;
    
    // Internal helper methods
    void buildAdjacencyList();
    bool hasPathDFS(NodeID from, NodeID to, std::set<NodeID>& visited) const;
    void removeNodeFromGraph(NodeID nodeID);
    void addEdge(NodeID from, NodeID to);
    void removeEdge(NodeID from, NodeID to);
};

} // namespace P2POverlay

#endif // TOPOLOGY_MANAGER_H

