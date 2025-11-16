#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include "TopologyManager.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <queue>
#include <chrono>

namespace P2POverlay {

/**
 * Routing strategy
 */
enum class RoutingStrategy {
    DIRECT,      // Direct delivery if connected
    SHORTEST_PATH, // Shortest path routing
    FLOOD,        // Flood routing
    GOSSIP        // Gossip protocol
};

/**
 * Message routing information
 */
struct RoutingInfo {
    std::vector<NodeID> path;
    int hopCount;
    uint64_t timestamp;
    RoutingStrategy strategy;
    
    RoutingInfo() : hopCount(0), timestamp(0), strategy(RoutingStrategy::DIRECT) {}
};

/**
 * Routes messages through the overlay network
 */
class MessageRouter {
public:
    MessageRouter(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager,
        std::shared_ptr<TopologyManager> topologyManager
    );
    ~MessageRouter();
    
    // Message routing
    bool routeMessage(const Message& message, RoutingStrategy strategy = RoutingStrategy::SHORTEST_PATH);
    bool routeMessageDirect(NodeID targetID, const Message& message);
    bool routeMessageMultiHop(NodeID targetID, const Message& message);
    bool floodMessage(const Message& message, int maxHops = 5);
    
    // Routing queries
    std::vector<NodeID> findRoute(NodeID targetID) const;
    int getHopCount(NodeID targetID) const;
    bool isReachable(NodeID targetID) const;
    
    // Message forwarding
    bool forwardMessage(const Message& message, const RoutingInfo& routingInfo);
    void handleIncomingRoute(const Message& message, const RoutingInfo& routingInfo);
    
    // Routing table management
    void updateRoutingTable();
    void clearRoutingTable();
    std::map<NodeID, std::vector<NodeID>> getRoutingTable() const;
    
    // Statistics
    size_t getRoutedMessageCount() const { return routedMessageCount_; }
    size_t getForwardedMessageCount() const { return forwardedMessageCount_; }
    double getAverageHopCount() const;
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<TopologyManager> topologyManager_;
    
    // Routing table: destination -> next hop
    mutable std::mutex routingTableMutex_;
    std::map<NodeID, NodeID> routingTable_;
    std::map<NodeID, int> hopCounts_;
    std::map<NodeID, std::chrono::system_clock::time_point> routeTimestamps_;
    
    // Message tracking for flood prevention
    mutable std::mutex seenMessagesMutex_;
    std::map<uint64_t, std::chrono::system_clock::time_point> seenMessages_;
    
    // Statistics
    std::atomic<size_t> routedMessageCount_;
    std::atomic<size_t> forwardedMessageCount_;
    std::atomic<size_t> totalHopCount_;
    
    // Internal methods
    std::vector<NodeID> computeShortestPath(NodeID targetID) const;
    NodeID getNextHop(NodeID targetID) const;
    bool isMessageSeen(uint64_t messageID) const;
    void markMessageSeen(uint64_t messageID);
    void cleanupSeenMessages(int timeoutSeconds = 300);
    uint64_t generateMessageID(const Message& message) const;
};

} // namespace P2POverlay

#endif // MESSAGE_ROUTER_H

