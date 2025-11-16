#ifndef NODE_SIMULATOR_H
#define NODE_SIMULATOR_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include "TopologyManager.h"
#include "MessageHandler.h"
#include "NodeDiscovery.h"
#include "NodeRegistration.h"
#include "DynamicNodeManager.h"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace P2POverlay {

/**
 * Simulated node for testing purposes
 */
class SimulatedNode {
public:
    SimulatedNode(NodeID id, Port port);
    ~SimulatedNode();
    
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Node operations
    bool joinNetwork(const NetworkAddress& bootstrapAddress);
    void leaveNetwork();
    
    // Getters
    NodeID getID() const;
    NetworkAddress getAddress() const;
    std::shared_ptr<Node> getNode() const { return node_; }
    std::shared_ptr<NetworkManager> getNetworkManager() const { return networkManager_; }
    std::shared_ptr<TopologyManager> getTopologyManager() const { return topologyManager_; }
    std::shared_ptr<DynamicNodeManager> getDynamicNodeManager() const { return dynamicNodeManager_; }
    
private:
    NodeID nodeID_;
    NetworkAddress address_;
    std::atomic<bool> running_;
    
    // Components
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<TopologyManager> topologyManager_;
    std::shared_ptr<MessageHandler> messageHandler_;
    std::shared_ptr<NodeDiscovery> nodeDiscovery_;
    std::shared_ptr<NodeRegistration> nodeRegistration_;
    std::shared_ptr<DynamicNodeManager> dynamicNodeManager_;
    
    // Thread
    std::thread nodeThread_;
    void nodeThreadFunction();
};

/**
 * Network simulator for testing multiple nodes
 */
class NetworkSimulator {
public:
    NetworkSimulator();
    ~NetworkSimulator();
    
    // Node management
    SimulatedNode* createNode(Port port);
    bool removeNode(NodeID nodeID);
    SimulatedNode* getNode(NodeID nodeID);
    
    // Network operations
    void startAllNodes();
    void stopAllNodes();
    void simulateNetworkActivity(int durationSeconds);
    
    // Testing scenarios
    void testNodeDiscovery();
    void testNodeRegistration();
    void testDynamicNodeAddition();
    void testNodeRemoval();
    void testNodeFailure();
    void testNetworkIntegrity();
    
    // Statistics
    size_t getNodeCount() const;
    std::vector<NodeID> getAllNodeIDs() const;
    void printNetworkStatus() const;
    
private:
    std::vector<std::unique_ptr<SimulatedNode>> nodes_;
    std::map<NodeID, SimulatedNode*> nodeMap_;
    std::atomic<bool> running_;
    
    NodeID generateNodeID();
};

} // namespace P2POverlay

#endif // NODE_SIMULATOR_H

