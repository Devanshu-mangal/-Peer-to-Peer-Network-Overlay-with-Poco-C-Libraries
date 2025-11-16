#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Common.h"
#include "Node.h"
#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Thread.h>
#include <Poco/Runnable.h>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <map>

namespace P2POverlay {

/**
 * Manages network communication for the P2P overlay
 */
class NetworkManager {
public:
    NetworkManager(std::shared_ptr<Node> node);
    ~NetworkManager();
    
    // Server operations
    bool startServer(Port port);
    void stopServer();
    bool isServerRunning() const;
    
    // Client operations
    bool connectToPeer(const NetworkAddress& peerAddress);
    bool disconnectFromPeer(NodeID peerID);
    
    // Message sending
    bool sendMessageToPeer(NodeID peerID, const Message& message);
    bool broadcastMessage(const Message& message, NodeID excludeID = 0);
    
    // Message receiving callback
    void setMessageCallback(std::function<void(const Message&)> callback);
    
    // Connection management
    std::vector<NodeID> getConnectedPeers() const;
    bool isConnectedTo(NodeID peerID) const;
    
    // Network statistics
    size_t getSentMessageCount() const { return sentMessageCount_; }
    size_t getReceivedMessageCount() const { return receivedMessageCount_; }
    
private:
    std::shared_ptr<Node> node_;
    
    // Server components
    std::unique_ptr<Poco::Net::TCPServer> tcpServer_;
    Poco::Thread serverThread_;
    std::atomic<bool> serverRunning_;
    
    // Connection management
    mutable std::mutex connectionsMutex_;
    std::map<NodeID, Poco::Net::StreamSocket> activeConnections_;
    
    // Message handling
    std::function<void(const Message&)> messageCallback_;
    std::queue<Message> incomingMessages_;
    mutable std::mutex messageQueueMutex_;
    
    // Statistics
    std::atomic<size_t> sentMessageCount_;
    std::atomic<size_t> receivedMessageCount_;
    
    // Internal helper methods
    void handleIncomingConnection(Poco::Net::StreamSocket& socket);
    bool serializeMessage(const Message& msg, std::vector<uint8_t>& buffer);
    bool deserializeMessage(const std::vector<uint8_t>& buffer, Message& msg);
    
    // Allow PeerConnectionHandler to access messageCallback_
    friend class PeerConnectionHandler;
};

/**
 * TCP Server Connection Handler
 */
class PeerConnectionHandler : public Poco::Net::TCPServerConnection {
public:
    PeerConnectionHandler(const Poco::Net::StreamSocket& socket, NetworkManager* networkManager);
    void run() override;
    
private:
    NetworkManager* networkManager_;
    friend class NetworkManager;
};

/**
 * Connection Factory
 */
class PeerConnectionFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    PeerConnectionFactory(NetworkManager* networkManager);
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override;
    
private:
    NetworkManager* networkManager_;
};

} // namespace P2POverlay

#endif // NETWORK_MANAGER_H

