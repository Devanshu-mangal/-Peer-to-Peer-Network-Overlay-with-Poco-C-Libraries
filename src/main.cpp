#include "Node.h"
#include "NetworkManager.h"
#include "TopologyManager.h"
#include "MessageHandler.h"
#include "NodeDiscovery.h"
#include "NodeRegistration.h"
#include "DynamicNodeManager.h"
#include "MessageRouter.h"
#include "DataExchange.h"
#include "ReliableMessaging.h"
#include "Common.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <limits>
#include <Poco/Net/DNS.h>

using namespace P2POverlay;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <port> [bootstrap_host] [bootstrap_port]" << std::endl;
    std::cout << "  port: Local port to listen on" << std::endl;
    std::cout << "  bootstrap_host: Optional bootstrap node hostname" << std::endl;
    std::cout << "  bootstrap_port: Optional bootstrap node port" << std::endl;
}

// Forward declarations
void printNodeInfo(std::shared_ptr<Node> node, 
                   std::shared_ptr<NetworkManager> networkManager,
                   std::shared_ptr<TopologyManager> topologyManager,
                   std::shared_ptr<DynamicNodeManager> dynamicNodeManager,
                   std::shared_ptr<MessageRouter> messageRouter,
                   std::shared_ptr<ReliableMessaging> reliableMessaging,
                   std::shared_ptr<DataExchange> dataExchange);
void handleDiscoveryOption(int option,
                          std::shared_ptr<NodeDiscovery> nodeDiscovery,
                          std::shared_ptr<TopologyManager> topologyManager);
void handleRegistrationOption(int option,
                             std::shared_ptr<NodeRegistration> nodeRegistration,
                             std::shared_ptr<TopologyManager> topologyManager);
void handleNodeManagementOption(int option,
                                std::shared_ptr<Node> node,
                                std::shared_ptr<DynamicNodeManager> dynamicNodeManager,
                                std::shared_ptr<TopologyManager> topologyManager);
void handleRoutingOption(int option,
                        std::shared_ptr<Node> node,
                        std::shared_ptr<MessageRouter> messageRouter,
                        std::shared_ptr<MessageHandler> messageHandler);
void handleReliableMessagingOption(int option,
                                   std::shared_ptr<Node> node,
                                   std::shared_ptr<ReliableMessaging> reliableMessaging,
                                   std::shared_ptr<MessageHandler> messageHandler);
void handleDataExchangeOption(int option,
                              std::shared_ptr<Node> node,
                              std::shared_ptr<DataExchange> dataExchange);

void printMainMenu() {
    std::cout << "\n=== Main Menu ===" << std::endl;
    std::cout << "  1. Node Discovery" << std::endl;
    std::cout << "  2. Node Registration" << std::endl;
    std::cout << "  3. Dynamic Node Management" << std::endl;
    std::cout << "  4. Message Routing" << std::endl;
    std::cout << "  5. Reliable Messaging" << std::endl;
    std::cout << "  6. Data Exchange" << std::endl;
    std::cout << "  7. Exit" << std::endl;
    std::cout << "\nEnter option number: ";
}

void printDiscoveryMenu() {
    std::cout << "\n=== Node Discovery Menu ===" << std::endl;
    std::cout << "  1. Discover Network (via bootstrap)" << std::endl;
    std::cout << "  2. Discover Peers" << std::endl;
    std::cout << "  3. Show Discovered Nodes" << std::endl;
    std::cout << "  4. Start Periodic Discovery" << std::endl;
    std::cout << "  5. Stop Periodic Discovery" << std::endl;
    std::cout << "  0. Back to Main Menu" << std::endl;
    std::cout << "\nEnter option number: ";
}

void printRegistrationMenu() {
    std::cout << "\n=== Node Registration Menu ===" << std::endl;
    std::cout << "  1. Register with Network" << std::endl;
    std::cout << "  2. Register with Specific Node" << std::endl;
    std::cout << "  3. Check Registration Status" << std::endl;
    std::cout << "  4. View Pending Registrations" << std::endl;
    std::cout << "  0. Back to Main Menu" << std::endl;
    std::cout << "\nEnter option number: ";
}

void printNodeManagementMenu() {
    std::cout << "\n=== Dynamic Node Management Menu ===" << std::endl;
    std::cout << "  1. Add Node" << std::endl;
    std::cout << "  2. Remove Node" << std::endl;
    std::cout << "  3. Show All Nodes" << std::endl;
    std::cout << "  4. Show Node Info" << std::endl;
    std::cout << "  5. Detect Failed Nodes" << std::endl;
    std::cout << "  6. Maintain Network Integrity" << std::endl;
    std::cout << "  7. Show Network Statistics" << std::endl;
    std::cout << "  0. Back to Main Menu" << std::endl;
    std::cout << "\nEnter option number: ";
}

void printRoutingMenu() {
    std::cout << "\n=== Message Routing Menu ===" << std::endl;
    std::cout << "  1. Send Message (Shortest Path)" << std::endl;
    std::cout << "  2. Send Message (Direct)" << std::endl;
    std::cout << "  3. Flood Message" << std::endl;
    std::cout << "  4. Show Routing Table" << std::endl;
    std::cout << "  5. Update Routing Table" << std::endl;
    std::cout << "  6. Check Node Reachability" << std::endl;
    std::cout << "  0. Back to Main Menu" << std::endl;
    std::cout << "\nEnter option number: ";
}

void printReliableMessagingMenu() {
    std::cout << "\n=== Reliable Messaging Menu ===" << std::endl;
    std::cout << "  1. Send Reliable Message" << std::endl;
    std::cout << "  2. Check Message Status" << std::endl;
    std::cout << "  3. Retry Pending Messages" << std::endl;
    std::cout << "  4. Show Statistics" << std::endl;
    std::cout << "  0. Back to Main Menu" << std::endl;
    std::cout << "\nEnter option number: ";
}

void printDataExchangeMenu() {
    std::cout << "\n=== Data Exchange Menu ===" << std::endl;
    std::cout << "  1. Send Data" << std::endl;
    std::cout << "  2. Check Transfer Status" << std::endl;
    std::cout << "  3. Cancel Transfer" << std::endl;
    std::cout << "  4. Show Active Transfers" << std::endl;
    std::cout << "  5. Get Received Data" << std::endl;
    std::cout << "  6. Show Statistics" << std::endl;
    std::cout << "  0. Back to Main Menu" << std::endl;
    std::cout << "\nEnter option number: ";
}

void handleMainMenuOption(int option,
                         std::shared_ptr<Node> node,
                         std::shared_ptr<NetworkManager> /*networkManager*/,
                         std::shared_ptr<TopologyManager> topologyManager,
                         std::shared_ptr<MessageHandler> messageHandler,
                         std::shared_ptr<NodeDiscovery> nodeDiscovery,
                         std::shared_ptr<NodeRegistration> nodeRegistration,
                         std::shared_ptr<DynamicNodeManager> dynamicNodeManager,
                         std::shared_ptr<MessageRouter> messageRouter,
                         std::shared_ptr<ReliableMessaging> reliableMessaging,
                         std::shared_ptr<DataExchange> dataExchange,
                         bool& running) {
    switch (option) {
        case 1: {
            // Node Discovery submenu
            int subOption;
            do {
                printDiscoveryMenu();
                std::cin >> subOption;
                handleDiscoveryOption(subOption, nodeDiscovery, topologyManager);
            } while (subOption != 0);
            if (running) printMainMenu();
            break;
        }
        case 2: {
            // Node Registration submenu
            int subOption;
            do {
                printRegistrationMenu();
                std::cin >> subOption;
                handleRegistrationOption(subOption, nodeRegistration, topologyManager);
            } while (subOption != 0);
            if (running) printMainMenu();
            break;
        }
        case 3: {
            // Dynamic Node Management submenu
            int subOption;
            do {
                printNodeManagementMenu();
                std::cin >> subOption;
                handleNodeManagementOption(subOption, node, dynamicNodeManager, topologyManager);
            } while (subOption != 0);
            if (running) printMainMenu();
            break;
        }
        case 4: {
            // Message Routing submenu
            int subOption;
            do {
                printRoutingMenu();
                std::cin >> subOption;
                handleRoutingOption(subOption, node, messageRouter, messageHandler);
            } while (subOption != 0);
            if (running) printMainMenu();
            break;
        }
        case 5: {
            // Reliable Messaging submenu
            int subOption;
            do {
                printReliableMessagingMenu();
                std::cin >> subOption;
                handleReliableMessagingOption(subOption, node, reliableMessaging, messageHandler);
            } while (subOption != 0);
            if (running) printMainMenu();
            break;
        }
        case 6: {
            // Data Exchange submenu
            int subOption;
            do {
                printDataExchangeMenu();
                std::cin >> subOption;
                handleDataExchangeOption(subOption, node, dataExchange);
            } while (subOption != 0);
            if (running) printMainMenu();
            break;
        }
        case 7: {
            running = false;
            break;
        }
        default: {
            std::cout << "\nInvalid option. Please try again." << std::endl;
            if (running) printMainMenu();
            break;
        }
    }
}

void handleDiscoveryOption(int option,
                          std::shared_ptr<NodeDiscovery> nodeDiscovery,
                          std::shared_ptr<TopologyManager> topologyManager) {
    switch (option) {
        case 1: {
            std::cout << "\nEnter bootstrap host: ";
            std::string host;
            std::cin >> host;
            std::cout << "Enter bootstrap port: ";
            Port port;
            std::cin >> port;
            NetworkAddress bootstrapAddr(host, port);
            std::vector<NetworkAddress> bootstrapNodes = {bootstrapAddr};
            if (nodeDiscovery->discoverNetwork(bootstrapNodes)) {
                std::cout << "Network discovery successful!" << std::endl;
            } else {
                std::cout << "Network discovery failed." << std::endl;
            }
            break;
        }
        case 2: {
            std::cout << "\nDiscovering peers..." << std::endl;
            std::vector<NodeID> peers = nodeDiscovery->discoverPeers(MAX_PEERS);
            std::cout << "Discovered " << peers.size() << " peer(s)" << std::endl;
            for (NodeID peerID : peers) {
                NetworkAddress addr = topologyManager->getNodeAddress(peerID);
                std::cout << "  - Node " << peerID << " at " << addr.toString() << std::endl;
            }
            break;
        }
        case 3: {
            std::vector<NodeID> discovered = nodeDiscovery->getDiscoveredNodes();
            std::cout << "\nDiscovered Nodes: " << discovered.size() << std::endl;
            for (NodeID nodeID : discovered) {
                NetworkAddress addr = topologyManager->getNodeAddress(nodeID);
                std::cout << "  - Node " << nodeID << " at " << addr.toString() << std::endl;
            }
            break;
        }
        case 4: {
            std::cout << "Enter discovery interval (seconds): ";
            int interval;
            std::cin >> interval;
            nodeDiscovery->startPeriodicDiscovery(interval);
            std::cout << "Periodic discovery started." << std::endl;
            break;
        }
        case 5: {
            nodeDiscovery->stopPeriodicDiscovery();
            std::cout << "Periodic discovery stopped." << std::endl;
            break;
        }
        case 0:
            break;
        default:
            std::cout << "Invalid option." << std::endl;
    }
}

void handleRegistrationOption(int option,
                              std::shared_ptr<NodeRegistration> nodeRegistration,
                              std::shared_ptr<TopologyManager> topologyManager) {
    switch (option) {
        case 1: {
            std::cout << "\nEnter bootstrap host: ";
            std::string host;
            std::cin >> host;
            std::cout << "Enter bootstrap port: ";
            Port port;
            std::cin >> port;
            NetworkAddress bootstrapAddr(host, port);
            if (nodeRegistration->registerWithNetwork(bootstrapAddr)) {
                std::cout << "Registration successful!" << std::endl;
            } else {
                std::cout << "Registration failed." << std::endl;
            }
            break;
        }
        case 2: {
            std::cout << "\nEnter target node ID: ";
            NodeID targetID;
            std::cin >> targetID;
            NetworkAddress targetAddr = topologyManager->getNodeAddress(targetID);
            if (targetAddr.port != 0) {
                if (nodeRegistration->registerWithNode(targetID, targetAddr)) {
                    std::cout << "Registration with node " << targetID << " successful!" << std::endl;
                } else {
                    std::cout << "Registration failed." << std::endl;
                }
            } else {
                std::cout << "Node not found." << std::endl;
            }
            break;
        }
        case 3: {
            if (nodeRegistration->isRegistered()) {
                std::cout << "\nStatus: REGISTERED" << std::endl;
            } else {
                std::cout << "\nStatus: NOT REGISTERED" << std::endl;
            }
            break;
        }
        case 4: {
            auto pending = nodeRegistration->getPendingRegistrations();
            std::cout << "\nPending Registrations: " << pending.size() << std::endl;
            for (const auto& req : pending) {
                std::cout << "  - Node " << req.nodeID << " at " << req.address.toString() << std::endl;
            }
            break;
        }
        case 0:
            break;
        default:
            std::cout << "Invalid option." << std::endl;
    }
}

void handleNodeManagementOption(int option,
                                std::shared_ptr<Node> /*node*/,
                                std::shared_ptr<DynamicNodeManager> dynamicNodeManager,
                                std::shared_ptr<TopologyManager> topologyManager) {
    switch (option) {
        case 1: {
            std::cout << "\nEnter new node ID (or 0 for random): ";
            NodeID newNodeID;
            std::cin >> newNodeID;
            if (newNodeID == 0) {
                std::random_device rd;
                std::mt19937_64 gen(rd());
                std::uniform_int_distribution<NodeID> dis;
                newNodeID = dis(gen);
            }
            std::cout << "Enter node host: ";
            std::string host;
            std::cin >> host;
            std::cout << "Enter node port: ";
            Port port;
            std::cin >> port;
            NetworkAddress newNodeAddr(host, port);
            if (dynamicNodeManager->addNode(newNodeID, newNodeAddr)) {
                std::cout << "Node " << newNodeID << " added successfully!" << std::endl;
            } else {
                std::cout << "Failed to add node." << std::endl;
            }
            break;
        }
        case 2: {
            std::cout << "\nEnter node ID to remove: ";
            NodeID nodeID;
            std::cin >> nodeID;
            std::cout << "Graceful removal? (1=yes, 0=no): ";
            int graceful;
            std::cin >> graceful;
            if (dynamicNodeManager->removeNode(nodeID, graceful == 1)) {
                std::cout << "Node " << nodeID << " removed." << std::endl;
            } else {
                std::cout << "Failed to remove node." << std::endl;
            }
            break;
        }
        case 3: {
            std::cout << "\n=== All Nodes ===" << std::endl;
            auto allNodes = dynamicNodeManager->getAllNodeInfo();
            for (const auto& nodeInfo : allNodes) {
                std::cout << "Node " << nodeInfo.nodeID << " at " << nodeInfo.address.toString();
                std::cout << " (State: " << static_cast<int>(nodeInfo.state) << ")" << std::endl;
            }
            break;
        }
        case 4: {
            std::cout << "\nEnter node ID: ";
            NodeID nodeID;
            std::cin >> nodeID;
            NodeInfo info = dynamicNodeManager->getNodeInfo(nodeID);
            if (info.nodeID != 0) {
                std::cout << "Node ID: " << info.nodeID << std::endl;
                std::cout << "Address: " << info.address.toString() << std::endl;
                std::cout << "State: " << static_cast<int>(info.state) << std::endl;
                std::cout << "Failure Count: " << info.failureCount << std::endl;
            } else {
                std::cout << "Node not found." << std::endl;
            }
            break;
        }
        case 5: {
            std::cout << "\nDetecting failed nodes..." << std::endl;
            dynamicNodeManager->detectFailedNodes(NODE_TIMEOUT_SEC);
            auto failed = dynamicNodeManager->getFailedNodes();
            std::cout << "Found " << failed.size() << " failed node(s)" << std::endl;
            break;
        }
        case 6: {
            std::cout << "\nMaintaining network integrity..." << std::endl;
            dynamicNodeManager->maintainNetworkIntegrity();
            std::cout << "Network integrity maintenance complete." << std::endl;
            break;
        }
        case 7: {
            std::cout << "\n=== Network Statistics ===" << std::endl;
            std::cout << "Active Nodes: " << dynamicNodeManager->getActiveNodeCount() << std::endl;
            std::cout << "Failed Nodes: " << dynamicNodeManager->getFailedNodeCount() << std::endl;
            std::cout << "Network Size: " << topologyManager->getNetworkSize() << std::endl;
            break;
        }
        case 0:
            break;
        default:
            std::cout << "Invalid option." << std::endl;
    }
}

void handleRoutingOption(int option,
                        std::shared_ptr<Node> node,
                        std::shared_ptr<MessageRouter> messageRouter,
                        std::shared_ptr<MessageHandler> /*messageHandler*/) {
    switch (option) {
        case 1: {
            std::cout << "\nEnter target node ID: ";
            NodeID targetID;
            std::cin >> targetID;
            std::cout << "Enter message text: ";
            std::string text;
            std::cin.ignore();
            std::getline(std::cin, text);
            Message msg;
            msg.type = MessageType::DATA_MESSAGE;
            msg.senderID = node->getID();
            msg.receiverID = targetID;
            msg.payload.assign(text.begin(), text.end());
            msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            if (messageRouter->routeMessage(msg, RoutingStrategy::SHORTEST_PATH)) {
                std::cout << "Message routed to node " << targetID << std::endl;
            } else {
                std::cout << "Failed to route message." << std::endl;
            }
            break;
        }
        case 2: {
            std::cout << "\nEnter target node ID: ";
            NodeID targetID;
            std::cin >> targetID;
            std::cout << "Enter message text: ";
            std::string text;
            std::cin.ignore();
            std::getline(std::cin, text);
            Message msg;
            msg.type = MessageType::DATA_MESSAGE;
            msg.senderID = node->getID();
            msg.receiverID = targetID;
            msg.payload.assign(text.begin(), text.end());
            msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            if (messageRouter->routeMessage(msg, RoutingStrategy::DIRECT)) {
                std::cout << "Message sent directly to node " << targetID << std::endl;
            } else {
                std::cout << "Failed to send message." << std::endl;
            }
            break;
        }
        case 3: {
            std::cout << "\nEnter message text: ";
            std::string text;
            std::cin.ignore();
            std::getline(std::cin, text);
            Message msg;
            msg.type = MessageType::DATA_MESSAGE;
            msg.senderID = node->getID();
            msg.receiverID = 0; // Broadcast
            msg.payload.assign(text.begin(), text.end());
            msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            if (messageRouter->floodMessage(msg, 5)) {
                std::cout << "Message flooded to network" << std::endl;
            } else {
                std::cout << "Failed to flood message." << std::endl;
            }
            break;
        }
        case 4: {
            auto routingTable = messageRouter->getRoutingTable();
            std::cout << "\n=== Routing Table ===" << std::endl;
            for (const auto& pair : routingTable) {
                std::cout << "To Node " << pair.first << ": ";
                for (size_t i = 0; i < pair.second.size(); ++i) {
                    std::cout << pair.second[i];
                    if (i < pair.second.size() - 1) std::cout << " -> ";
                }
                std::cout << std::endl;
            }
            break;
        }
        case 5: {
            messageRouter->updateRoutingTable();
            std::cout << "Routing table updated." << std::endl;
            break;
        }
        case 6: {
            std::cout << "\nEnter node ID to check: ";
            NodeID targetID;
            std::cin >> targetID;
            if (messageRouter->isReachable(targetID)) {
                int hops = messageRouter->getHopCount(targetID);
                std::cout << "Node " << targetID << " is reachable in " << hops << " hop(s)" << std::endl;
            } else {
                std::cout << "Node " << targetID << " is not reachable" << std::endl;
            }
            break;
        }
        case 0:
            break;
        default:
            std::cout << "Invalid option." << std::endl;
    }
}

void handleReliableMessagingOption(int option,
                                   std::shared_ptr<Node> node,
                                   std::shared_ptr<ReliableMessaging> reliableMessaging,
                                   std::shared_ptr<MessageHandler> /*messageHandler*/) {
    switch (option) {
        case 1: {
            std::cout << "\nEnter target node ID: ";
            NodeID targetID;
            std::cin >> targetID;
            std::cout << "Enter message text: ";
            std::string text;
            std::cin.ignore();
            std::getline(std::cin, text);
            Message msg;
            msg.type = MessageType::DATA_MESSAGE;
            msg.senderID = node->getID();
            msg.receiverID = targetID;
            msg.payload.assign(text.begin(), text.end());
            msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            uint64_t msgID = reliableMessaging->sendReliableMessage(targetID, msg);
            std::cout << "Reliable message sent (ID: " << msgID << ")" << std::endl;
            break;
        }
        case 2: {
            std::cout << "\nEnter message ID: ";
            uint64_t msgID;
            std::cin >> msgID;
            if (reliableMessaging->isMessageAcknowledged(msgID)) {
                std::cout << "Message " << msgID << " was acknowledged." << std::endl;
            } else {
                std::cout << "Message " << msgID << " is pending." << std::endl;
            }
            break;
        }
        case 3: {
            std::cout << "\nRetrying pending messages..." << std::endl;
            reliableMessaging->retryPendingMessages(30, 3);
            std::cout << "Retry complete." << std::endl;
            break;
        }
        case 4: {
            std::cout << "\n=== Reliable Messaging Statistics ===" << std::endl;
            std::cout << "Sent: " << reliableMessaging->getSentMessages() << std::endl;
            std::cout << "Acknowledged: " << reliableMessaging->getAcknowledgedMessages() << std::endl;
            std::cout << "Failed: " << reliableMessaging->getFailedMessages() << std::endl;
            std::cout << "Delivery Rate: " << reliableMessaging->getDeliveryRate() << "%" << std::endl;
            break;
        }
        case 0:
            break;
        default:
            std::cout << "Invalid option." << std::endl;
    }
}

void handleDataExchangeOption(int option,
                              std::shared_ptr<Node> /*node*/,
                              std::shared_ptr<DataExchange> dataExchange) {
    switch (option) {
        case 1: {
            std::cout << "\nEnter target node ID: ";
            NodeID targetID;
            std::cin >> targetID;
            std::cout << "Enter data size in bytes: ";
            size_t dataSize;
            std::cin >> dataSize;
            std::cout << "Enter data type (or press Enter for 'generic'): ";
            std::string dataType;
            std::cin.ignore();
            std::getline(std::cin, dataType);
            if (dataType.empty()) dataType = "generic";
            std::vector<uint8_t> data(dataSize, 0x42);
            uint64_t transferID = dataExchange->sendData(targetID, data, dataType);
            std::cout << "Transfer started (ID: " << transferID << ")" << std::endl;
            break;
        }
        case 2: {
            std::cout << "\nEnter transfer ID: ";
            uint64_t transferID;
            std::cin >> transferID;
            DataTransfer transfer = dataExchange->getTransferInfo(transferID);
            if (transfer.transferID != 0) {
                std::cout << "Transfer ID: " << transfer.transferID << std::endl;
                std::cout << "Status: " << static_cast<int>(transfer.status) << std::endl;
                std::cout << "Progress: " << transfer.transferredSize << "/" << transfer.totalSize << " bytes" << std::endl;
            } else {
                std::cout << "Transfer not found." << std::endl;
            }
            break;
        }
        case 3: {
            std::cout << "\nEnter transfer ID to cancel: ";
            uint64_t transferID;
            std::cin >> transferID;
            if (dataExchange->cancelTransfer(transferID)) {
                std::cout << "Transfer " << transferID << " cancelled." << std::endl;
            } else {
                std::cout << "Failed to cancel transfer." << std::endl;
            }
            break;
        }
        case 4: {
            auto transfers = dataExchange->getActiveTransfers();
            std::cout << "\nActive Transfers: " << transfers.size() << std::endl;
            for (const auto& transfer : transfers) {
                std::cout << "  Transfer " << transfer.transferID << ": " 
                          << transfer.transferredSize << "/" << transfer.totalSize << " bytes" << std::endl;
            }
            break;
        }
        case 5: {
            std::cout << "\nEnter transfer ID: ";
            uint64_t transferID;
            std::cin >> transferID;
            if (dataExchange->isTransferComplete(transferID)) {
                auto data = dataExchange->getReceivedData(transferID);
                std::cout << "Received " << data.size() << " bytes" << std::endl;
            } else {
                std::cout << "Transfer not complete yet." << std::endl;
            }
            break;
        }
        case 6: {
            std::cout << "\n=== Data Exchange Statistics ===" << std::endl;
            std::cout << "Data Sent: " << (dataExchange->getSentDataSize() / 1024) << " KB" << std::endl;
            std::cout << "Data Received: " << (dataExchange->getReceivedDataSize() / 1024) << " KB" << std::endl;
            std::cout << "Completed: " << dataExchange->getCompletedTransfers() << std::endl;
            std::cout << "Failed: " << dataExchange->getFailedTransfers() << std::endl;
            break;
        }
        case 0:
            break;
        default:
            std::cout << "Invalid option." << std::endl;
    }
}

void printNodeInfo(std::shared_ptr<Node> node, 
                   std::shared_ptr<NetworkManager> networkManager,
                   std::shared_ptr<TopologyManager> topologyManager,
                   std::shared_ptr<DynamicNodeManager> dynamicNodeManager = nullptr,
                   std::shared_ptr<MessageRouter> messageRouter = nullptr,
                   std::shared_ptr<ReliableMessaging> reliableMessaging = nullptr,
                   std::shared_ptr<DataExchange> dataExchange = nullptr) {
    std::cout << "\n=== Node Information ===" << std::endl;
    std::cout << "Node ID: " << node->getID() << std::endl;
    std::cout << "Address: " << node->getAddress().toString() << std::endl;
    std::cout << "Status: " << (node->isActive() ? "Active" : "Inactive") << std::endl;
    std::cout << "Connected Peers: " << node->getPeerCount() << std::endl;
    std::cout << "Network Size: " << topologyManager->getNetworkSize() << std::endl;
    std::cout << "Server Running: " << (networkManager->isServerRunning() ? "Yes" : "No") << std::endl;
    std::cout << "Messages Sent: " << networkManager->getSentMessageCount() << std::endl;
    std::cout << "Messages Received: " << networkManager->getReceivedMessageCount() << std::endl;
    
    if (dynamicNodeManager) {
        std::cout << "Active Nodes: " << dynamicNodeManager->getActiveNodeCount() << std::endl;
        std::cout << "Failed Nodes: " << dynamicNodeManager->getFailedNodeCount() << std::endl;
    }
    
    if (messageRouter) {
        std::cout << "Routed Messages: " << messageRouter->getRoutedMessageCount() << std::endl;
        std::cout << "Forwarded Messages: " << messageRouter->getForwardedMessageCount() << std::endl;
        std::cout << "Average Hop Count: " << messageRouter->getAverageHopCount() << std::endl;
    }
    
    if (reliableMessaging) {
        std::cout << "Reliable Messages Sent: " << reliableMessaging->getSentMessages() << std::endl;
        std::cout << "Acknowledged: " << reliableMessaging->getAcknowledgedMessages() << std::endl;
        std::cout << "Delivery Rate: " << reliableMessaging->getDeliveryRate() << "%" << std::endl;
    }
    
    if (dataExchange) {
        std::cout << "Data Sent: " << (dataExchange->getSentDataSize() / 1024) << " KB" << std::endl;
        std::cout << "Data Received: " << (dataExchange->getReceivedDataSize() / 1024) << " KB" << std::endl;
        std::cout << "Completed Transfers: " << dataExchange->getCompletedTransfers() << std::endl;
    }
    
    std::vector<NodeID> peerIDs = node->getPeerIDs();
    if (!peerIDs.empty()) {
        std::cout << "\nConnected Peers:" << std::endl;
        for (NodeID peerID : peerIDs) {
            NetworkAddress addr = topologyManager->getNodeAddress(peerID);
            std::cout << "  - Node " << peerID << " at " << addr.toString() << std::endl;
        }
    }
    std::cout << "========================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    Port port = static_cast<Port>(std::stoi(argv[1]));
    
    // Generate a unique node ID (in production, use a proper ID generation scheme)
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<NodeID> dis;
    NodeID nodeID = dis(gen);
    
    // Get local hostname
    std::string hostname = "localhost";
    try {
        hostname = Poco::Net::DNS::hostName();
    } catch (...) {
        // Use default if hostname resolution fails
    }
    
    NetworkAddress nodeAddress(hostname, port);
    
    // Create node
    std::shared_ptr<Node> node = std::make_shared<Node>(nodeID, nodeAddress);
    
    // Create network manager
    std::shared_ptr<NetworkManager> networkManager = std::make_shared<NetworkManager>(node);
    
    // Create topology manager
    std::shared_ptr<TopologyManager> topologyManager = std::make_shared<TopologyManager>(node);
    
    // Register local node in topology
    topologyManager->addNode(nodeID, nodeAddress);
    
    // Create message handler
    std::shared_ptr<MessageHandler> messageHandler = std::make_shared<MessageHandler>(
        node, networkManager, topologyManager
    );
    
    // Create advanced components
    std::shared_ptr<NodeDiscovery> nodeDiscovery = std::make_shared<NodeDiscovery>(
        node, networkManager, topologyManager
    );
    
    std::shared_ptr<NodeRegistration> nodeRegistration = std::make_shared<NodeRegistration>(
        node, networkManager, topologyManager
    );
    
    std::shared_ptr<DynamicNodeManager> dynamicNodeManager = std::make_shared<DynamicNodeManager>(
        node, networkManager, topologyManager
    );
    
    std::shared_ptr<MessageRouter> messageRouter = std::make_shared<MessageRouter>(
        node, networkManager, topologyManager
    );
    
    std::shared_ptr<ReliableMessaging> reliableMessaging = std::make_shared<ReliableMessaging>(
        node, networkManager
    );
    
    std::shared_ptr<DataExchange> dataExchange = std::make_shared<DataExchange>(
        node, networkManager, messageRouter
    );
    
    // Set up discovery callbacks (silent - user can check status manually)
    nodeDiscovery->setOnPeerDiscoveredCallback([dynamicNodeManager](NodeID id, const NetworkAddress& addr) {
        // Automatically add discovered peers (silently)
        if (dynamicNodeManager) {
            dynamicNodeManager->addNodeWithValidation(id, addr);
        }
    });
    
    // Set up registration callbacks (silent)
    nodeRegistration->setOnRegistrationSuccessCallback([](NodeID /*id*/, const NetworkAddress& /*addr*/) {
        // Registration successful (silent)
    });
    
    // Set up dynamic node manager callbacks (silent)
    dynamicNodeManager->setOnNodeAddedCallback([](NodeID /*id*/, const NetworkAddress& /*addr*/) {
        // Node added (silent)
    });
    
    dynamicNodeManager->setOnNodeRemovedCallback([](NodeID /*id*/) {
        // Node removed (silent)
    });
    
    dynamicNodeManager->setOnNodeFailedCallback([](NodeID /*id*/) {
        // Node failed (silent)
    });
    
    dynamicNodeManager->setOnNetworkRepairedCallback([]() {
        // Network repaired (silent)
    });
    
    // Set up reliable messaging callbacks (silent)
    reliableMessaging->setOnMessageDeliveredCallback([](uint64_t /*msgID*/, NodeID /*target*/) {
        // Message delivered (silent)
    });
    
    reliableMessaging->setOnMessageFailedCallback([](uint64_t /*msgID*/, NodeID /*target*/) {
        // Message failed (silent)
    });
    
    // Set up data exchange callbacks (silent)
    dataExchange->setOnDataReceivedCallback([](NodeID /*sourceID*/, const std::vector<uint8_t>& /*data*/, const std::string& /*dataType*/) {
        // Data received (silent)
    });
    
    dataExchange->setOnTransferCompleteCallback([](uint64_t /*transferID*/, bool /*success*/) {
        // Transfer complete (silent)
    });
    
    dataExchange->setOnTransferProgressCallback([](uint64_t /*transferID*/, size_t /*current*/, size_t /*total*/) {
        // Transfer progress (silent)
    });
    
    // Set up message callback (enhanced to handle all message types)
    networkManager->setMessageCallback([messageHandler, dataExchange, reliableMessaging](const Message& msg) {
        // Handle data chunks
        if (msg.type == MessageType::DATA_CHUNK && dataExchange) {
            // Parse chunk from payload (simplified - in real implementation, proper deserialization)
            // For now, let MessageHandler process it
        }
        
        // Handle acknowledgments
        if (msg.type == MessageType::MESSAGE_ACK && reliableMessaging) {
            // Extract message ID from payload and acknowledge
            // For now, let MessageHandler process it
        }
        
        // Process message normally
        messageHandler->processMessage(msg);
    });
    
    // Update routing table periodically
    messageRouter->updateRoutingTable();
    
    // Start server
    std::cout << "Starting P2P Overlay Network Node..." << std::endl;
    std::cout << "Node ID: " << nodeID << std::endl;
    std::cout << "Listening on: " << nodeAddress.toString() << std::endl;
    
    if (!networkManager->startServer(port)) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }
    
    // Connect to bootstrap node if provided (using advanced components)
    if (argc >= 4) {
        std::string bootstrapHost = argv[2];
        Port bootstrapPort = static_cast<Port>(std::stoi(argv[3]));
        NetworkAddress bootstrapAddr(bootstrapHost, bootstrapPort);
        
        // Use NodeDiscovery to discover network
        std::vector<NetworkAddress> bootstrapNodes = {bootstrapAddr};
        nodeDiscovery->discoverNetwork(bootstrapNodes);
        nodeRegistration->registerWithNetwork(bootstrapAddr);
    }
    
    // Start background services (silently)
    dynamicNodeManager->startFailureDetection(30);
    nodeDiscovery->startPeriodicDiscovery(60);
    
    // Main loop
    bool running = true;
    auto lastHeartbeat = std::chrono::steady_clock::now();
    auto lastStatusUpdate = std::chrono::steady_clock::now();
    
    std::cout << "\n=== P2P Overlay Network Node Running ===" << std::endl;
    std::cout << "Node ID: " << nodeID << std::endl;
    std::cout << "Address: " << nodeAddress.toString() << std::endl;
    std::cout << "Port: " << port << std::endl;
    
    auto lastRoutingUpdate = std::chrono::steady_clock::now();
    auto lastMaintenance = std::chrono::steady_clock::now();
    auto lastCleanup = std::chrono::steady_clock::now();
    
    // Print main menu
    std::cout << "\n=== Main Menu ===" << std::endl;
    std::cout << "  1. Node Discovery" << std::endl;
    std::cout << "  2. Node Registration" << std::endl;
    std::cout << "  3. Dynamic Node Management" << std::endl;
    std::cout << "  4. Message Routing" << std::endl;
    std::cout << "  5. Reliable Messaging" << std::endl;
    std::cout << "  6. Data Exchange" << std::endl;
    std::cout << "  7. Exit" << std::endl;
    std::cout << "\nEnter option number: ";
    
    // Input thread for menu handling
    std::thread inputThread([&running, node, networkManager, topologyManager, messageHandler,
                            nodeDiscovery, nodeRegistration, dynamicNodeManager, 
                            messageRouter, reliableMessaging, dataExchange]() {
        int option;
        while (running) {
            if (std::cin >> option) {
                handleMainMenuOption(option, node, networkManager, topologyManager, messageHandler,
                                   nodeDiscovery, nodeRegistration, dynamicNodeManager,
                                   messageRouter, reliableMessaging, dataExchange, running);
                if (running && option != 7) {
                    std::cout << "\nEnter option number: ";
                }
            } else {
                // Clear error state
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }
    });
    
    while (running) {
        auto now = std::chrono::steady_clock::now();
        
        // Send periodic heartbeats (using reliable messaging)
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() >= HEARTBEAT_INTERVAL_SEC) {
            std::vector<NodeID> peerIDs = node->getPeerIDs();
            for (NodeID peerID : peerIDs) {
                Message heartbeat = messageHandler->createHeartbeat(peerID);
                reliableMessaging->sendReliableMessage(peerID, heartbeat);
            }
            lastHeartbeat = now;
        }
        
        // Update routing table periodically
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastRoutingUpdate).count() >= 30) {
            messageRouter->updateRoutingTable();
            lastRoutingUpdate = now;
        }
        
        // Network maintenance (background)
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastMaintenance).count() >= 60) {
            dynamicNodeManager->maintainNetworkIntegrity();
            lastMaintenance = now;
        }
        
        // Cleanup operations (background)
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCleanup).count() >= 300) {
            reliableMessaging->cleanupAcknowledgedMessages(300);
            reliableMessaging->retryPendingMessages(30, 3);
            dataExchange->cleanupCompletedTransfers(3600);
            lastCleanup = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Wait for input thread
    if (inputThread.joinable()) {
        inputThread.join();
    }
    
    // Cleanup
    std::cout << "\nShutting down..." << std::endl;
    
    // Stop advanced components
    nodeDiscovery->stopPeriodicDiscovery();
    dynamicNodeManager->stopFailureDetection();
    
    // Send leave notifications to all peers (using reliable messaging)
    std::vector<NodeID> peerIDs = node->getPeerIDs();
    for (NodeID peerID : peerIDs) {
        Message leaveMsg = messageHandler->createLeaveNotification(peerID);
        reliableMessaging->sendReliableMessage(peerID, leaveMsg);
    }
    
    // Gracefully remove this node
    dynamicNodeManager->removeNodeGracefully(node->getID());
    
    // Stop server
    networkManager->stopServer();
    node->setActive(false);
    
    // Print summary
    std::cout << "\n=== Session Summary ===" << std::endl;
    std::cout << "Node ID: " << node->getID() << std::endl;
    std::cout << "Address: " << node->getAddress().toString() << std::endl;
    std::cout << "Port: " << node->getAddress().port << std::endl;
    std::cout << "Total Messages Sent: " << networkManager->getSentMessageCount() << std::endl;
    std::cout << "Total Messages Received: " << networkManager->getReceivedMessageCount() << std::endl;
    std::cout << "Routed Messages: " << messageRouter->getRoutedMessageCount() << std::endl;
    std::cout << "Reliable Messages: " << reliableMessaging->getSentMessages() << std::endl;
    std::cout << "Delivery Rate: " << reliableMessaging->getDeliveryRate() << "%" << std::endl;
    std::cout << "Data Transferred: " << (dataExchange->getSentDataSize() / 1024) << " KB sent, " 
              << (dataExchange->getReceivedDataSize() / 1024) << " KB received" << std::endl;
    std::cout << "Network Size: " << topologyManager->getNetworkSize() << " nodes" << std::endl;
    std::cout << "Active Nodes: " << dynamicNodeManager->getActiveNodeCount() << std::endl;
    std::cout << "========================\n" << std::endl;
    
    std::cout << "Node shutdown complete." << std::endl;
    
    return 0;
}

