#ifndef DYNAMIC_NODE_MANAGER_H
#define DYNAMIC_NODE_MANAGER_H

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
 * Node state in the network
 */
enum class NodeState {
    JOINING,
    ACTIVE,
    LEAVING,
    FAILED,
    UNKNOWN
};

/**
 * Node information with state tracking
 */
struct NodeInfo {
    NodeID nodeID;
    NetworkAddress address;
    NodeState state;
    std::chrono::system_clock::time_point lastSeen;
    std::chrono::system_clock::time_point joinTime;
    int failureCount;
    
    NodeInfo() : nodeID(0), state(NodeState::UNKNOWN), failureCount(0) {}
};

/**
 * Manages dynamic addition and removal of nodes with integrity maintenance
 */
class DynamicNodeManager {
public:
    DynamicNodeManager(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager,
        std::shared_ptr<TopologyManager> topologyManager
    );
    ~DynamicNodeManager();
    
    // Node addition
    bool addNode(NodeID nodeID, const NetworkAddress& address);
    bool addNodeWithValidation(NodeID nodeID, const NetworkAddress& address);
    std::vector<NodeID> addNodesFromList(const std::vector<std::pair<NodeID, NetworkAddress>>& nodes);
    
    // Node removal
    bool removeNode(NodeID nodeID, bool graceful = true);
    bool removeNodeGracefully(NodeID nodeID);
    bool removeNodeForced(NodeID nodeID);
    
    // Node state management
    NodeState getNodeState(NodeID nodeID) const;
    void setNodeState(NodeID nodeID, NodeState state);
    std::vector<NodeID> getNodesByState(NodeState state) const;
    
    // Failure detection and recovery
    void detectFailedNodes(int timeoutSeconds = NODE_TIMEOUT_SEC);
    std::vector<NodeID> getFailedNodes() const;
    bool recoverFromNodeFailure(NodeID failedNodeID);
    void startFailureDetection(int intervalSeconds = 30);
    void stopFailureDetection();
    
    // Network integrity
    bool maintainNetworkIntegrity();
    bool repairNetworkAfterNodeRemoval(NodeID removedNodeID);
    bool ensureConnectivity();
    
    // Topology updates
    void propagateTopologyUpdate(const std::vector<NodeID>& updatedNodes);
    void handleTopologyChange(NodeID changedNodeID, bool added);
    
    // Statistics and monitoring
    size_t getActiveNodeCount() const;
    size_t getFailedNodeCount() const;
    std::vector<NodeInfo> getAllNodeInfo() const;
    NodeInfo getNodeInfo(NodeID nodeID) const;
    
    // Callbacks
    void setOnNodeAddedCallback(std::function<void(NodeID, const NetworkAddress&)> callback);
    void setOnNodeRemovedCallback(std::function<void(NodeID)> callback);
    void setOnNodeFailedCallback(std::function<void(NodeID)> callback);
    void setOnNetworkRepairedCallback(std::function<void()> callback);
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<TopologyManager> topologyManager_;
    
    // Node tracking
    mutable std::mutex nodesMutex_;
    std::map<NodeID, NodeInfo> nodeRegistry_;
    
    // Failure detection
    std::atomic<bool> failureDetectionActive_;
    
    // Callbacks
    std::function<void(NodeID, const NetworkAddress&)> onNodeAdded_;
    std::function<void(NodeID)> onNodeRemoved_;
    std::function<void(NodeID)> onNodeFailed_;
    std::function<void()> onNetworkRepaired_;
    
    // Internal methods
    bool validateNodeAddition(NodeID nodeID, const NetworkAddress& address) const;
    void updateNodeLastSeen(NodeID nodeID);
    void incrementFailureCount(NodeID nodeID);
    void resetFailureCount(NodeID nodeID);
    bool shouldRemoveNode(NodeID nodeID) const;
    void notifyNodeRemoval(NodeID nodeID);
    std::vector<NodeID> findReplacementConnections(NodeID removedNodeID) const;
    bool establishReplacementConnections(const std::vector<NodeID>& replacements);
};

} // namespace P2POverlay

#endif // DYNAMIC_NODE_MANAGER_H

