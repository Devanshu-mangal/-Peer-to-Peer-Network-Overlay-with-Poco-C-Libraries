#ifndef NODE_REGISTRATION_H
#define NODE_REGISTRATION_H

#include "Common.h"
#include "Node.h"
#include "NetworkManager.h"
#include "TopologyManager.h"
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>

namespace P2POverlay {

/**
 * Registration status
 */
enum class RegistrationStatus {
    PENDING,
    REGISTERED,
    REJECTED,
    FAILED
};

/**
 * Registration request information
 */
struct RegistrationRequest {
    NodeID nodeID;
    NetworkAddress address;
    std::string nodeInfo;  // Optional node metadata
    uint64_t timestamp;
    RegistrationStatus status;
    
    RegistrationRequest() : nodeID(0), timestamp(0), status(RegistrationStatus::PENDING) {}
};

/**
 * Handles node registration with validation and security
 */
class NodeRegistration {
public:
    NodeRegistration(
        std::shared_ptr<Node> node,
        std::shared_ptr<NetworkManager> networkManager,
        std::shared_ptr<TopologyManager> topologyManager
    );
    ~NodeRegistration();
    
    // Registration operations
    bool registerWithNetwork(const NetworkAddress& bootstrapAddress);
    bool registerWithNode(NodeID targetNodeID, const NetworkAddress& targetAddress);
    
    // Registration handling (for accepting registrations)
    bool handleRegistrationRequest(const RegistrationRequest& request);
    bool acceptRegistration(NodeID nodeID, const NetworkAddress& address);
    bool rejectRegistration(NodeID nodeID, const std::string& reason);
    
    // Registration status
    RegistrationStatus getRegistrationStatus() const { return registrationStatus_; }
    bool isRegistered() const { return registrationStatus_ == RegistrationStatus::REGISTERED; }
    
    // Validation and security
    bool validateRegistrationRequest(const RegistrationRequest& request) const;
    bool isNodeAuthorized(NodeID nodeID) const;
    void setAuthorizationCallback(std::function<bool(NodeID, const NetworkAddress&)> callback);
    
    // Registration callbacks
    void setOnRegistrationSuccessCallback(std::function<void(NodeID, const NetworkAddress&)> callback);
    void setOnRegistrationFailedCallback(std::function<void(const std::string&)> callback);
    
    // Pending registrations (for nodes accepting registrations)
    std::vector<RegistrationRequest> getPendingRegistrations() const;
    void processPendingRegistrations();
    
private:
    std::shared_ptr<Node> node_;
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<TopologyManager> topologyManager_;
    
    // Registration state
    std::atomic<RegistrationStatus> registrationStatus_;
    mutable std::mutex pendingRegistrationsMutex_;
    std::map<NodeID, RegistrationRequest> pendingRegistrations_;
    
    // Validation
    std::function<bool(NodeID, const NetworkAddress&)> authorizationCallback_;
    
    // Callbacks
    std::function<void(NodeID, const NetworkAddress&)> onRegistrationSuccess_;
    std::function<void(const std::string&)> onRegistrationFailed_;
    
    // Internal methods
    bool performRegistrationHandshake(NodeID targetNodeID, const NetworkAddress& targetAddress);
    bool validateNodeID(NodeID nodeID) const;
    bool validateAddress(const NetworkAddress& address) const;
    std::string generateRegistrationToken(NodeID nodeID) const;
};

} // namespace P2POverlay

#endif // NODE_REGISTRATION_H

