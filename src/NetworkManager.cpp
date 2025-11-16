#include "NetworkManager.h"
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Exception.h>
#include <iostream>
#include <cstring>
#include <map>

namespace P2POverlay {

// PeerConnectionHandler implementation
PeerConnectionHandler::PeerConnectionHandler(
    const Poco::Net::StreamSocket& socket, 
    NetworkManager* networkManager)
    : TCPServerConnection(socket), networkManager_(networkManager) {
}

void PeerConnectionHandler::run() {
    try {
        Poco::Net::StreamSocket& socket = this->socket();
        
        // Read message header (type, senderID, receiverID, timestamp, payload size)
        char header[32];
        int received = socket.receiveBytes(header, sizeof(header));
        
        if (received > 0 && networkManager_) {
            Message msg;
            // Parse header
            std::memcpy(&msg.type, header, sizeof(MessageType));
            std::memcpy(&msg.senderID, header + sizeof(MessageType), sizeof(NodeID));
            std::memcpy(&msg.receiverID, header + sizeof(MessageType) + sizeof(NodeID), sizeof(NodeID));
            std::memcpy(&msg.timestamp, header + sizeof(MessageType) + 2 * sizeof(NodeID), sizeof(uint64_t));
            
            uint32_t payloadSize = 0;
            std::memcpy(&payloadSize, header + sizeof(MessageType) + 2 * sizeof(NodeID) + sizeof(uint64_t), sizeof(uint32_t));
            
            // Read payload if present
            if (payloadSize > 0) {
                msg.payload.resize(payloadSize);
                socket.receiveBytes(msg.payload.data(), payloadSize);
            }
            
            if (networkManager_->messageCallback_) {
                networkManager_->messageCallback_(msg);
            }
        }
    } catch (Poco::Exception& e) {
        std::cerr << "Connection handler error: " << e.displayText() << std::endl;
    }
}

// PeerConnectionFactory implementation
PeerConnectionFactory::PeerConnectionFactory(NetworkManager* networkManager)
    : networkManager_(networkManager) {
}

Poco::Net::TCPServerConnection* PeerConnectionFactory::createConnection(
    const Poco::Net::StreamSocket& socket) {
    return new PeerConnectionHandler(socket, networkManager_);
}

// NetworkManager implementation
NetworkManager::NetworkManager(std::shared_ptr<Node> node)
    : node_(node), serverRunning_(false), sentMessageCount_(0), receivedMessageCount_(0) {
}

NetworkManager::~NetworkManager() {
    stopServer();
}

bool NetworkManager::startServer(Port port) {
    try {
        if (serverRunning_) {
            return false;
        }
        
        Poco::Net::ServerSocket serverSocket(port);
        serverSocket.setReuseAddress(true);
        serverSocket.setReusePort(true);
        
        auto factory = std::make_unique<PeerConnectionFactory>(this);
        tcpServer_ = std::make_unique<Poco::Net::TCPServer>(factory.release(), serverSocket);
        tcpServer_->start();
        
        serverRunning_ = true;
        return true;
    } catch (Poco::Exception& e) {
        std::cerr << "Failed to start server: " << e.displayText() << std::endl;
        return false;
    }
}

void NetworkManager::stopServer() {
    if (tcpServer_ && serverRunning_) {
        tcpServer_->stop();
        serverRunning_ = false;
    }
}

bool NetworkManager::isServerRunning() const {
    return serverRunning_;
}

bool NetworkManager::connectToPeer(const NetworkAddress& peerAddress) {
    try {
        Poco::Net::SocketAddress address(peerAddress.host, peerAddress.port);
        Poco::Net::StreamSocket socket;
        socket.connect(address);
        socket.setNoDelay(true);
        
        // Store connection (we need to identify the peer ID somehow)
        // For now, we'll use a placeholder - in real implementation, 
        // we'd exchange node IDs during handshake
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        // Note: This is simplified - actual implementation would map addresses to node IDs
        return true;
    } catch (Poco::Exception& e) {
        std::cerr << "Failed to connect to peer: " << e.displayText() << std::endl;
        return false;
    }
}

bool NetworkManager::disconnectFromPeer(NodeID peerID) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = activeConnections_.find(peerID);
    if (it != activeConnections_.end()) {
        it->second.close();
        activeConnections_.erase(it);
        return true;
    }
    return false;
}

bool NetworkManager::sendMessageToPeer(NodeID peerID, const Message& message) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = activeConnections_.find(peerID);
    if (it == activeConnections_.end()) {
        return false;
    }
    
    try {
        Poco::Net::StreamSocket& socket = it->second;
        
        // Send message header
        char header[32];
        std::memcpy(header, &message.type, sizeof(MessageType));
        std::memcpy(header + sizeof(MessageType), &message.senderID, sizeof(NodeID));
        std::memcpy(header + sizeof(MessageType) + sizeof(NodeID), &message.receiverID, sizeof(NodeID));
        std::memcpy(header + sizeof(MessageType) + 2 * sizeof(NodeID), &message.timestamp, sizeof(uint64_t));
        
        uint32_t payloadSize = static_cast<uint32_t>(message.payload.size());
        std::memcpy(header + sizeof(MessageType) + 2 * sizeof(NodeID) + sizeof(uint64_t), &payloadSize, sizeof(uint32_t));
        
        socket.sendBytes(header, sizeof(header));
        
        // Send payload if present
        if (payloadSize > 0) {
            socket.sendBytes(message.payload.data(), payloadSize);
        }
        
        sentMessageCount_++;
        return true;
    } catch (Poco::Exception& e) {
        std::cerr << "Failed to send message: " << e.displayText() << std::endl;
        return false;
    }
}

bool NetworkManager::broadcastMessage(const Message& message, NodeID excludeID) {
    std::vector<NodeID> peerIDs = node_->getPeerIDs();
    bool success = true;
    
    for (NodeID peerID : peerIDs) {
        if (peerID != excludeID) {
            if (!sendMessageToPeer(peerID, message)) {
                success = false;
            }
        }
    }
    
    return success;
}

void NetworkManager::setMessageCallback(std::function<void(const Message&)> callback) {
    messageCallback_ = callback;
}

std::vector<NodeID> NetworkManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    std::vector<NodeID> peers;
    for (const auto& pair : activeConnections_) {
        peers.push_back(pair.first);
    }
    return peers;
}

bool NetworkManager::isConnectedTo(NodeID peerID) const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return activeConnections_.find(peerID) != activeConnections_.end();
}

void NetworkManager::handleIncomingConnection(Poco::Net::StreamSocket& /*socket*/) {
    // Handled by PeerConnectionHandler
}

bool NetworkManager::serializeMessage(const Message& msg, std::vector<uint8_t>& buffer) {
    // Serialization logic
    buffer.clear();
    buffer.resize(sizeof(MessageType) + 2 * sizeof(NodeID) + sizeof(uint64_t) + sizeof(uint32_t) + msg.payload.size());
    
    size_t offset = 0;
    std::memcpy(buffer.data() + offset, &msg.type, sizeof(MessageType));
    offset += sizeof(MessageType);
    std::memcpy(buffer.data() + offset, &msg.senderID, sizeof(NodeID));
    offset += sizeof(NodeID);
    std::memcpy(buffer.data() + offset, &msg.receiverID, sizeof(NodeID));
    offset += sizeof(NodeID);
    std::memcpy(buffer.data() + offset, &msg.timestamp, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    uint32_t payloadSize = static_cast<uint32_t>(msg.payload.size());
    std::memcpy(buffer.data() + offset, &payloadSize, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    if (!msg.payload.empty()) {
        std::memcpy(buffer.data() + offset, msg.payload.data(), msg.payload.size());
    }
    
    return true;
}

bool NetworkManager::deserializeMessage(const std::vector<uint8_t>& buffer, Message& msg) {
    if (buffer.size() < sizeof(MessageType) + 2 * sizeof(NodeID) + sizeof(uint64_t) + sizeof(uint32_t)) {
        return false;
    }
    
    size_t offset = 0;
    std::memcpy(&msg.type, buffer.data() + offset, sizeof(MessageType));
    offset += sizeof(MessageType);
    std::memcpy(&msg.senderID, buffer.data() + offset, sizeof(NodeID));
    offset += sizeof(NodeID);
    std::memcpy(&msg.receiverID, buffer.data() + offset, sizeof(NodeID));
    offset += sizeof(NodeID);
    std::memcpy(&msg.timestamp, buffer.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    uint32_t payloadSize = 0;
    std::memcpy(&payloadSize, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (payloadSize > 0 && buffer.size() >= offset + payloadSize) {
        msg.payload.resize(payloadSize);
        std::memcpy(msg.payload.data(), buffer.data() + offset, payloadSize);
    }
    
    return true;
}

} // namespace P2POverlay

