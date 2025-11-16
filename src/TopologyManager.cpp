#include "TopologyManager.h"
#include <algorithm>
#include <queue>
#include <map>
#include <set>

namespace P2POverlay {

TopologyManager::TopologyManager(std::shared_ptr<Node> node)
    : localNode_(node) {
}

TopologyManager::~TopologyManager() {
}

bool TopologyManager::addNode(NodeID nodeID, const NetworkAddress& address) {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    if (nodeRegistry_.find(nodeID) != nodeRegistry_.end()) {
        return false; // Node already exists
    }
    
    nodeRegistry_[nodeID] = address;
    adjacencyList_[nodeID] = std::set<NodeID>();
    
    return true;
}

bool TopologyManager::removeNode(NodeID nodeID) {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    if (nodeRegistry_.find(nodeID) == nodeRegistry_.end()) {
        return false; // Node doesn't exist
    }
    
    removeNodeFromGraph(nodeID);
    return true;
}

bool TopologyManager::updateNodeAddress(NodeID nodeID, const NetworkAddress& newAddress) {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    auto it = nodeRegistry_.find(nodeID);
    if (it == nodeRegistry_.end()) {
        return false;
    }
    
    it->second = newAddress;
    return true;
}

std::vector<NodeID> TopologyManager::discoverPeers(NodeID requestingNodeID, int maxPeers) {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    std::vector<NodeID> availablePeers;
    
    // Get all nodes except the requesting node
    for (const auto& pair : nodeRegistry_) {
        if (pair.first != requestingNodeID && pair.first != localNode_->getID()) {
            availablePeers.push_back(pair.first);
        }
    }
    
    // Limit the number of peers
    if (static_cast<int>(availablePeers.size()) > maxPeers) {
        availablePeers.resize(maxPeers);
    }
    
    return availablePeers;
}

bool TopologyManager::registerNode(NodeID nodeID, const NetworkAddress& address) {
    return addNode(nodeID, address);
}

bool TopologyManager::nodeExists(NodeID nodeID) const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    return nodeRegistry_.find(nodeID) != nodeRegistry_.end();
}

NetworkAddress TopologyManager::getNodeAddress(NodeID nodeID) const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    auto it = nodeRegistry_.find(nodeID);
    if (it != nodeRegistry_.end()) {
        return it->second;
    }
    return NetworkAddress();
}

std::vector<NodeID> TopologyManager::getAllNodeIDs() const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    std::vector<NodeID> nodeIDs;
    for (const auto& pair : nodeRegistry_) {
        nodeIDs.push_back(pair.first);
    }
    return nodeIDs;
}

size_t TopologyManager::getNetworkSize() const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    return nodeRegistry_.size();
}

std::vector<NodeID> TopologyManager::findPath(NodeID from, NodeID to) const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    if (from == to) {
        return {from};
    }
    
    // Simple BFS path finding
    std::queue<NodeID> queue;
    std::map<NodeID, NodeID> parent;
    std::set<NodeID> visited;
    
    queue.push(from);
    visited.insert(from);
    parent[from] = from;
    
    while (!queue.empty()) {
        NodeID current = queue.front();
        queue.pop();
        
        if (current == to) {
            // Reconstruct path
            std::vector<NodeID> path;
            NodeID node = to;
            while (node != from) {
                path.push_back(node);
                node = parent[node];
            }
            path.push_back(from);
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        auto it = adjacencyList_.find(current);
        if (it != adjacencyList_.end()) {
            for (NodeID neighbor : it->second) {
                if (visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    parent[neighbor] = current;
                    queue.push(neighbor);
                }
            }
        }
    }
    
    return {}; // No path found
}

std::vector<NodeID> TopologyManager::getNeighbors(NodeID nodeID) const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    auto it = adjacencyList_.find(nodeID);
    if (it != adjacencyList_.end()) {
        return std::vector<NodeID>(it->second.begin(), it->second.end());
    }
    return {};
}

void TopologyManager::updateTopology() {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    buildAdjacencyList();
}

void TopologyManager::validateTopology() {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    // Validate that all nodes in adjacency list exist in registry
    for (const auto& pair : adjacencyList_) {
        if (nodeRegistry_.find(pair.first) == nodeRegistry_.end()) {
            // Remove orphaned adjacency entries
            adjacencyList_.erase(pair.first);
        }
    }
}

bool TopologyManager::isTopologyConnected() const {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    if (nodeRegistry_.empty()) {
        return true; // Empty graph is considered connected
    }
    
    if (nodeRegistry_.size() == 1) {
        return true;
    }
    
    // Check if all nodes are reachable from the first node
    NodeID startNode = nodeRegistry_.begin()->first;
    std::set<NodeID> visited;
    hasPathDFS(startNode, startNode, visited);
    
    return visited.size() == nodeRegistry_.size();
}

bool TopologyManager::checkNetworkIntegrity() {
    validateTopology();
    return isTopologyConnected();
}

void TopologyManager::repairTopology() {
    std::lock_guard<std::mutex> lock(topologyMutex_);
    
    // Remove nodes that are no longer in registry from adjacency list
    validateTopology();
    
    // If topology is disconnected, try to reconnect
    if (!isTopologyConnected()) {
        // Simple repair: connect all nodes in a ring
        std::vector<NodeID> nodeIDs = getAllNodeIDs();
        if (nodeIDs.size() > 1) {
            for (size_t i = 0; i < nodeIDs.size(); ++i) {
                NodeID current = nodeIDs[i];
                NodeID next = nodeIDs[(i + 1) % nodeIDs.size()];
                addEdge(current, next);
            }
        }
    }
}

void TopologyManager::addBootstrapNode(const NetworkAddress& address) {
    bootstrapNodes_.push_back(address);
}

std::vector<NetworkAddress> TopologyManager::getBootstrapNodes() const {
    return bootstrapNodes_;
}

void TopologyManager::buildAdjacencyList() {
    // Adjacency list is built when edges are added/removed
    // This method can be used for additional processing if needed
}

bool TopologyManager::hasPathDFS(NodeID from, NodeID to, std::set<NodeID>& visited) const {
    visited.insert(from);
    
    if (from == to) {
        return true;
    }
    
    auto it = adjacencyList_.find(from);
    if (it != adjacencyList_.end()) {
        for (NodeID neighbor : it->second) {
            if (visited.find(neighbor) == visited.end()) {
                if (hasPathDFS(neighbor, to, visited)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

void TopologyManager::removeNodeFromGraph(NodeID nodeID) {
    // Remove from registry
    nodeRegistry_.erase(nodeID);
    
    // Remove from adjacency list
    adjacencyList_.erase(nodeID);
    
    // Remove all edges pointing to this node
    for (auto& pair : adjacencyList_) {
        pair.second.erase(nodeID);
    }
}

void TopologyManager::addEdge(NodeID from, NodeID to) {
    if (from == to) {
        return; // No self-loops
    }
    
    adjacencyList_[from].insert(to);
    adjacencyList_[to].insert(from); // Undirected graph
}

void TopologyManager::removeEdge(NodeID from, NodeID to) {
    auto itFrom = adjacencyList_.find(from);
    if (itFrom != adjacencyList_.end()) {
        itFrom->second.erase(to);
    }
    
    auto itTo = adjacencyList_.find(to);
    if (itTo != adjacencyList_.end()) {
        itTo->second.erase(from);
    }
}

} // namespace P2POverlay

