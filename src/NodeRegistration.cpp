#include "NodeRegistration.h"
#include "MessageHandler.h"
#include <Poco/Net/SocketAddress.h>
#include <Poco/Exception.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <atomic>
#include <mutex>

namespace P2POverlay {

NodeRegistration::NodeRegistration(
    std::shared_ptr<Node> node,
    std::shared_ptr<NetworkManager> networkManager,
    std::shared_ptr<TopologyManager> topologyManager)
    : node_(node), networkManager_(networkManager), topologyManager_(topologyManager),
      registrationStatus_(RegistrationStatus::PENDING) {
}

NodeRegistration::~NodeRegistration() {
}

bool NodeRegistration::registerWithNetwork(const NetworkAddress& bootstrapAddress) {
    std::cout << "Attempting to register with network via " << bootstrapAddress.toString() << std::endl;
    
    // First, try to connect to bootstrap node
    if (!networkManager_->connectToPeer(bootstrapAddress)) {
        std::cerr << "Failed to connect to bootstrap node" << std::endl;
        registrationStatus_ = RegistrationStatus::FAILED;
        if (onRegistrationFailed_) {
            onRegistrationFailed_("Connection to bootstrap node failed");
        }
        return false;
    }
    
    // In a real implementation, we'd need the bootstrap node's ID
    // For now, we'll use a simplified registration
    registrationStatus_ = RegistrationStatus::REGISTERED;
    
    // Register in topology
    topologyManager_->addNode(node_->getID(), node_->getAddress());
    
    std::cout << "Successfully registered with network" << std::endl;
    if (onRegistrationSuccess_) {
        onRegistrationSuccess_(node_->getID(), node_->getAddress());
    }
    
    return true;
}

bool NodeRegistration::registerWithNode(NodeID targetNodeID, const NetworkAddress& targetAddress) {
    if (!validateAddress(targetAddress)) {
        registrationStatus_ = RegistrationStatus::FAILED;
        return false;
    }
    
    // Create registration request
    RegistrationRequest request;
    request.nodeID = node_->getID();
    request.address = node_->getAddress();
    request.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    request.status = RegistrationStatus::PENDING;
    
    // Validate request
    if (!validateRegistrationRequest(request)) {
        registrationStatus_ = RegistrationStatus::REJECTED;
        return false;
    }
    
    // Perform handshake
    if (!performRegistrationHandshake(targetNodeID, targetAddress)) {
        registrationStatus_ = RegistrationStatus::FAILED;
        if (onRegistrationFailed_) {
            onRegistrationFailed_("Registration handshake failed");
        }
        return false;
    }
    
    // Send registration request (would use MessageHandler in real implementation)
    registrationStatus_ = RegistrationStatus::REGISTERED;
    
    if (onRegistrationSuccess_) {
        onRegistrationSuccess_(node_->getID(), node_->getAddress());
    }
    
    return true;
}

bool NodeRegistration::handleRegistrationRequest(const RegistrationRequest& request) {
    if (!validateRegistrationRequest(request)) {
        return false;
    }
    
    // Check authorization
    if (authorizationCallback_ && !authorizationCallback_(request.nodeID, request.address)) {
        rejectRegistration(request.nodeID, "Node not authorized");
        return false;
    }
    
    // Check if we can accept more peers
    if (node_->getPeerCount() >= MAX_PEERS) {
        rejectRegistration(request.nodeID, "Maximum peer limit reached");
        return false;
    }
    
    // Accept registration
    return acceptRegistration(request.nodeID, request.address);
}

bool NodeRegistration::acceptRegistration(NodeID nodeID, const NetworkAddress& address) {
    std::lock_guard<std::mutex> lock(pendingRegistrationsMutex_);
    
    // Add to topology
    if (!topologyManager_->addNode(nodeID, address)) {
        return false;
    }
    
    // Add as peer
    if (!node_->addPeer(nodeID, address)) {
        topologyManager_->removeNode(nodeID);
        return false;
    }
    
    // Update registration status
    auto it = pendingRegistrations_.find(nodeID);
    if (it != pendingRegistrations_.end()) {
        it->second.status = RegistrationStatus::REGISTERED;
    }
    
    std::cout << "Accepted registration from node " << nodeID << " at " << address.toString() << std::endl;
    
    return true;
}

bool NodeRegistration::rejectRegistration(NodeID nodeID, const std::string& reason) {
    std::lock_guard<std::mutex> lock(pendingRegistrationsMutex_);
    
    auto it = pendingRegistrations_.find(nodeID);
    if (it != pendingRegistrations_.end()) {
        it->second.status = RegistrationStatus::REJECTED;
    }
    
    std::cout << "Rejected registration from node " << nodeID << ": " << reason << std::endl;
    
    return true;
}

bool NodeRegistration::validateRegistrationRequest(const RegistrationRequest& request) const {
    // Validate node ID
    if (!validateNodeID(request.nodeID)) {
        return false;
    }
    
    // Validate address
    if (!validateAddress(request.address)) {
        return false;
    }
    
    // Check if node already exists
    if (topologyManager_->nodeExists(request.nodeID)) {
        return false;
    }
    
    // Check timestamp (prevent replay attacks)
    auto now = std::chrono::system_clock::now();
    auto requestTime = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(request.timestamp)
    );
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - requestTime);
    
    if (elapsed.count() > 60) { // Request too old
        return false;
    }
    
    return true;
}

bool NodeRegistration::isNodeAuthorized(NodeID nodeID) const {
    if (authorizationCallback_) {
        NetworkAddress addr = topologyManager_->getNodeAddress(nodeID);
        return authorizationCallback_(nodeID, addr);
    }
    
    // Default: all nodes authorized
    return true;
}

void NodeRegistration::setAuthorizationCallback(std::function<bool(NodeID, const NetworkAddress&)> callback) {
    authorizationCallback_ = callback;
}

void NodeRegistration::setOnRegistrationSuccessCallback(std::function<void(NodeID, const NetworkAddress&)> callback) {
    onRegistrationSuccess_ = callback;
}

void NodeRegistration::setOnRegistrationFailedCallback(std::function<void(const std::string&)> callback) {
    onRegistrationFailed_ = callback;
}

std::vector<RegistrationRequest> NodeRegistration::getPendingRegistrations() const {
    std::lock_guard<std::mutex> lock(pendingRegistrationsMutex_);
    std::vector<RegistrationRequest> requests;
    
    for (const auto& pair : pendingRegistrations_) {
        if (pair.second.status == RegistrationStatus::PENDING) {
            requests.push_back(pair.second);
        }
    }
    
    return requests;
}

void NodeRegistration::processPendingRegistrations() {
    std::vector<RegistrationRequest> pending = getPendingRegistrations();
    
    for (const auto& request : pending) {
        handleRegistrationRequest(request);
    }
}

bool NodeRegistration::performRegistrationHandshake(NodeID /*targetNodeID*/, const NetworkAddress& targetAddress) {
    // Connect to target node
    if (!networkManager_->connectToPeer(targetAddress)) {
        return false;
    }
    
    // In a real implementation, we'd exchange registration tokens
    // and perform mutual authentication
    
    return true;
}

bool NodeRegistration::validateNodeID(NodeID nodeID) const {
    // Basic validation: node ID should not be zero
    if (nodeID == 0) {
        return false;
    }
    
    // Node ID should not be our own (unless re-registering)
    if (nodeID == node_->getID()) {
        return false;
    }
    
    return true;
}

bool NodeRegistration::validateAddress(const NetworkAddress& address) const {
    if (address.host.empty() || address.port == 0) {
        return false;
    }
    
    // Check if port is in valid range
    if (address.port < 1024 || address.port > 65535) {
        return false;
    }
    
    // Check if it's not our own address
    if (address == node_->getAddress()) {
        return false;
    }
    
    return true;
}

std::string NodeRegistration::generateRegistrationToken(NodeID nodeID) const {
    // Simple token generation (in production, use proper cryptographic methods)
    std::stringstream ss;
    ss << std::hex << nodeID << "-" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return ss.str();
}

} // namespace P2POverlay

