#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <cstdint>
#include <vector>
#include <memory>

namespace P2POverlay {

// Node identifier type
using NodeID = uint64_t;

// Port number type
using Port = uint16_t;

// Message types
enum class MessageType : uint8_t {
    JOIN_REQUEST = 1,
    JOIN_RESPONSE = 2,
    LEAVE_NOTIFICATION = 3,
    HEARTBEAT = 4,
    DATA_MESSAGE = 5,
    TOPOLOGY_UPDATE = 6,
    PEER_DISCOVERY = 7,
    ROUTE_MESSAGE = 8,
    MESSAGE_ACK = 9,
    DATA_CHUNK = 10,
    TRANSFER_REQUEST = 11,
    TRANSFER_RESPONSE = 12
};

// Network configuration constants
constexpr Port DEFAULT_PORT = 8888;
constexpr int HEARTBEAT_INTERVAL_SEC = 30;
constexpr int NODE_TIMEOUT_SEC = 90;
constexpr int MAX_PEERS = 10;

// Network address structure
struct NetworkAddress {
    std::string host;
    Port port;
    
    NetworkAddress() : host(""), port(0) {}
    NetworkAddress(const std::string& h, Port p) : host(h), port(p) {}
    
    bool operator==(const NetworkAddress& other) const {
        return host == other.host && port == other.port;
    }
    
    std::string toString() const {
        return host + ":" + std::to_string(port);
    }
};

// Message structure
struct Message {
    MessageType type;
    NodeID senderID;
    NodeID receiverID;
    std::vector<uint8_t> payload;
    uint64_t timestamp;
    
    Message() : type(MessageType::DATA_MESSAGE), senderID(0), receiverID(0), timestamp(0) {}
};

} // namespace P2POverlay

#endif // COMMON_H

