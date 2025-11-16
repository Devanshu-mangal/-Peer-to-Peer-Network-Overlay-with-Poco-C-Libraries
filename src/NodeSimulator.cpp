#include "NodeSimulator.h"
#include <Poco/Net/DNS.h>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include <map>

namespace P2POverlay {

// SimulatedNode implementation
SimulatedNode::SimulatedNode(NodeID id, Port port)
    : nodeID_(id), running_(false) {
    std::string hostname = "localhost";
    try {
        hostname = Poco::Net::DNS::hostName();
    } catch (...) {
    }
    address_ = NetworkAddress(hostname, port);
    
    // Create components
    node_ = std::make_shared<Node>(nodeID_, address_);
    networkManager_ = std::make_shared<NetworkManager>(node_);
    topologyManager_ = std::make_shared<TopologyManager>(node_);
    
    // Register local node
    topologyManager_->addNode(nodeID_, address_);
    
    messageHandler_ = std::make_shared<MessageHandler>(node_, networkManager_, topologyManager_);
    nodeDiscovery_ = std::make_shared<NodeDiscovery>(node_, networkManager_, topologyManager_);
    nodeRegistration_ = std::make_shared<NodeRegistration>(node_, networkManager_, topologyManager_);
    dynamicNodeManager_ = std::make_shared<DynamicNodeManager>(node_, networkManager_, topologyManager_);
    
    // Set up message callback
    networkManager_->setMessageCallback([this](const Message& msg) {
        messageHandler_->processMessage(msg);
    });
}

SimulatedNode::~SimulatedNode() {
    stop();
}

bool SimulatedNode::start() {
    if (running_) {
        return false;
    }
    
    // Start server
    if (!networkManager_->startServer(address_.port)) {
        return false;
    }
    
    running_ = true;
    nodeThread_ = std::thread(&SimulatedNode::nodeThreadFunction, this);
    
    return true;
}

void SimulatedNode::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Leave network gracefully
    leaveNetwork();
    
    // Stop server
    networkManager_->stopServer();
    
    // Wait for thread
    if (nodeThread_.joinable()) {
        nodeThread_.join();
    }
}

bool SimulatedNode::joinNetwork(const NetworkAddress& bootstrapAddress) {
    if (bootstrapAddress.port == 0) {
        // First node, no bootstrap needed
        return true;
    }
    
    // Discover network
    std::vector<NetworkAddress> bootstrapNodes = {bootstrapAddress};
    if (!nodeDiscovery_->discoverNetwork(bootstrapNodes)) {
        return false;
    }
    
    // Register with network
    if (!nodeRegistration_->registerWithNetwork(bootstrapAddress)) {
        return false;
    }
    
    return true;
}

void SimulatedNode::leaveNetwork() {
    // Send leave notifications
    std::vector<NodeID> peers = node_->getPeerIDs();
    for (NodeID peerID : peers) {
        Message leaveMsg = messageHandler_->createLeaveNotification(peerID);
        networkManager_->sendMessageToPeer(peerID, leaveMsg);
    }
    
    // Remove from topology
    dynamicNodeManager_->removeNode(nodeID_, true);
}

NodeID SimulatedNode::getID() const {
    return nodeID_;
}

NetworkAddress SimulatedNode::getAddress() const {
    return address_;
}

void SimulatedNode::nodeThreadFunction() {
    auto lastHeartbeat = std::chrono::steady_clock::now();
    auto lastMaintenance = std::chrono::steady_clock::now();
    
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        
        // Send heartbeats
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() >= HEARTBEAT_INTERVAL_SEC) {
            std::vector<NodeID> peers = node_->getPeerIDs();
            for (NodeID peerID : peers) {
                Message heartbeat = messageHandler_->createHeartbeat(peerID);
                networkManager_->sendMessageToPeer(peerID, heartbeat);
            }
            lastHeartbeat = now;
        }
        
        // Network maintenance
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastMaintenance).count() >= 60) {
            dynamicNodeManager_->maintainNetworkIntegrity();
            lastMaintenance = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// NetworkSimulator implementation
NetworkSimulator::NetworkSimulator() : running_(false) {
}

NetworkSimulator::~NetworkSimulator() {
    stopAllNodes();
}

SimulatedNode* NetworkSimulator::createNode(Port port) {
    NodeID nodeID = generateNodeID();
    auto node = std::make_unique<SimulatedNode>(nodeID, port);
    SimulatedNode* nodePtr = node.get();
    
    nodes_.push_back(std::move(node));
    nodeMap_[nodeID] = nodePtr;
    
    return nodePtr;
}

bool NetworkSimulator::removeNode(NodeID nodeID) {
    auto it = nodeMap_.find(nodeID);
    if (it == nodeMap_.end()) {
        return false;
    }
    
    SimulatedNode* node = it->second;
    node->stop();
    node->leaveNetwork();
    
    nodeMap_.erase(it);
    
    // Remove from vector
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
            [nodeID](const std::unique_ptr<SimulatedNode>& n) {
                return n->getID() == nodeID;
            }),
        nodes_.end()
    );
    
    return true;
}

SimulatedNode* NetworkSimulator::getNode(NodeID nodeID) {
    auto it = nodeMap_.find(nodeID);
    if (it != nodeMap_.end()) {
        return it->second;
    }
    return nullptr;
}

void NetworkSimulator::startAllNodes() {
    if (nodes_.empty()) {
        return;
    }
    
    // Start first node (bootstrap)
    if (nodes_.size() > 0) {
        nodes_[0]->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Start remaining nodes and connect to bootstrap
    NetworkAddress bootstrapAddr = nodes_[0]->getAddress();
    
    for (size_t i = 1; i < nodes_.size(); ++i) {
        nodes_[i]->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        nodes_[i]->joinNetwork(bootstrapAddr);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    running_ = true;
}

void NetworkSimulator::stopAllNodes() {
    running_ = false;
    
    for (auto& node : nodes_) {
        node->stop();
    }
}

void NetworkSimulator::simulateNetworkActivity(int durationSeconds) {
    std::cout << "Simulating network activity for " << durationSeconds << " seconds..." << std::endl;
    
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count() < durationSeconds) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void NetworkSimulator::testNodeDiscovery() {
    std::cout << "\n=== Testing Node Discovery ===" << std::endl;
    
    if (nodes_.size() < 2) {
        std::cout << "Need at least 2 nodes for discovery test" << std::endl;
        return;
    }
    
    SimulatedNode* node1 = nodes_[0].get();
    
    std::cout << "Node " << node1->getID() << " discovering peers..." << std::endl;
    // Discovery test would go here
}

void NetworkSimulator::testNodeRegistration() {
    std::cout << "\n=== Testing Node Registration ===" << std::endl;
    // Registration test
}

void NetworkSimulator::testDynamicNodeAddition() {
    std::cout << "\n=== Testing Dynamic Node Addition ===" << std::endl;
    
    if (nodes_.empty()) {
        return;
    }
    
    // Add a new node
    SimulatedNode* newNode = createNode(static_cast<Port>(8890 + nodes_.size()));
    newNode->start();
    
    NetworkAddress bootstrapAddr = nodes_[0]->getAddress();
    newNode->joinNetwork(bootstrapAddr);
    
    std::cout << "Added new node " << newNode->getID() << std::endl;
}

void NetworkSimulator::testNodeRemoval() {
    std::cout << "\n=== Testing Node Removal ===" << std::endl;
    
    if (nodes_.size() < 2) {
        return;
    }
    
    NodeID nodeToRemove = nodes_.back()->getID();
    std::cout << "Removing node " << nodeToRemove << std::endl;
    removeNode(nodeToRemove);
}

void NetworkSimulator::testNodeFailure() {
    std::cout << "\n=== Testing Node Failure ===" << std::endl;
    
    if (nodes_.size() < 2) {
        return;
    }
    
    // Simulate node failure by stopping it abruptly
    SimulatedNode* node = nodes_.back().get();
    std::cout << "Simulating failure of node " << node->getID() << std::endl;
    node->stop();
}

void NetworkSimulator::testNetworkIntegrity() {
    std::cout << "\n=== Testing Network Integrity ===" << std::endl;
    
    for (auto& node : nodes_) {
        if (node->isRunning()) {
            node->getDynamicNodeManager()->maintainNetworkIntegrity();
        }
    }
}

size_t NetworkSimulator::getNodeCount() const {
    return nodes_.size();
}

std::vector<NodeID> NetworkSimulator::getAllNodeIDs() const {
    std::vector<NodeID> nodeIDs;
    for (const auto& node : nodes_) {
        nodeIDs.push_back(node->getID());
    }
    return nodeIDs;
}

void NetworkSimulator::printNetworkStatus() const {
    std::cout << "\n=== Network Status ===" << std::endl;
    std::cout << "Total nodes: " << nodes_.size() << std::endl;
    
    for (const auto& node : nodes_) {
        std::cout << "  Node " << node->getID() 
                  << " at " << node->getAddress().toString()
                  << " (Running: " << (node->isRunning() ? "Yes" : "No") << ")"
                  << std::endl;
    }
}

NodeID NetworkSimulator::generateNodeID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<NodeID> dis;
    return dis(gen);
}

} // namespace P2POverlay

