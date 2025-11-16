# P2P Network Overlay

A robust peer-to-peer (P2P) network overlay implementation in C++ using Poco C++ libraries. This project provides a dynamic overlay network capable of accommodating the addition and removal of nodes while preserving network integrity and functionality.

## Requirements and Objectives

### Core Requirements

1. **Dynamic Node Management**
   - Support for adding new nodes to the network dynamically
   - Graceful handling of node removals and disconnections
   - Automatic peer discovery and connection establishment

2. **Network Integrity**
   - Maintain network connectivity even when nodes join or leave
   - Topology validation and repair mechanisms
   - Heartbeat monitoring to detect node failures

3. **Communication Protocol**
   - Reliable message passing between nodes
   - Support for multiple message types (join, leave, heartbeat, data, topology updates)
   - Broadcast and unicast messaging capabilities

4. **Scalability**
   - Support for multiple concurrent connections
   - Configurable maximum peer connections per node
   - Efficient routing and path finding

### Objectives

- Create a self-organizing P2P overlay network
- Implement robust error handling and recovery mechanisms
- Provide a clean, modular architecture for extensibility
- Ensure thread-safe operations for concurrent access
- Maintain network topology information across all nodes

## Architecture

### System Components

The P2P overlay network consists of the following core components:

#### 1. Node (`Node.h/cpp`)
Represents a peer node in the network with the following responsibilities:
- Maintains node identification (ID and network address)
- Manages peer connections and relationships
- Tracks heartbeat information for liveness detection
- Stores topology information

**Key Features:**
- Thread-safe peer management
- Heartbeat tracking for node liveness
- Configurable maximum peer connections

#### 2. NetworkManager (`NetworkManager.h/cpp`)
Handles all network communication operations:
- TCP server for accepting incoming connections
- Client connections to other peers
- Message serialization and deserialization
- Connection lifecycle management

**Key Features:**
- Asynchronous connection handling using Poco TCPServer
- Message queue management
- Connection statistics tracking
- Broadcast and unicast messaging

#### 3. TopologyManager (`TopologyManager.h/cpp`)
Manages the overlay network topology:
- Node registry and address mapping
- Adjacency list for network graph representation
- Path finding and routing
- Topology validation and repair

**Key Features:**
- Graph-based topology representation
- BFS path finding algorithm
- Network connectivity validation
- Bootstrap node management

#### 4. MessageHandler (`MessageHandler.h/cpp`)
Processes different types of network messages:
- Join/leave request handling
- Heartbeat processing
- Data message routing
- Topology update propagation

**Key Features:**
- Type-based message routing
- Message creation utilities
- Payload serialization/deserialization

### Communication Protocol

The network uses a custom message protocol with the following message types:

- **JOIN_REQUEST**: Request to join the network
- **JOIN_RESPONSE**: Response to join request with peer list
- **LEAVE_NOTIFICATION**: Notification of node departure
- **HEARTBEAT**: Periodic liveness check
- **DATA_MESSAGE**: Application data transmission
- **TOPOLOGY_UPDATE**: Network topology change notification
- **PEER_DISCOVERY**: Request for peer discovery

### Network Topology

The overlay maintains an undirected graph representation of the network:
- Each node maintains connections to a limited number of peers (configurable via `MAX_PEERS`)
- The topology is maintained through periodic updates and notifications
- Bootstrap nodes facilitate initial network entry for new nodes

### Node Lifecycle

1. **Initialization**: Node starts with a unique ID and listening port
2. **Bootstrap**: Connects to bootstrap nodes (if available) to discover the network
3. **Discovery**: Discovers and connects to other peers
4. **Operation**: Participates in network communication and topology maintenance
5. **Shutdown**: Sends leave notifications and gracefully disconnects

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or higher
- Poco C++ Libraries (Foundation, Net, Util components)

### Installing Poco C++ Libraries

#### Windows (using vcpkg)
```bash
vcpkg install poco
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install libpoco-dev
```

#### macOS (using Homebrew)
```bash
brew install poco
```

#### Building from Source
```bash
git clone https://github.com/pocoproject/poco.git
cd poco
./configure
make
sudo make install
```

### Building the Project

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Running

```bash
# Windows
.\build\Release\P2POverlayNetwork.exe 8888

# Linux/macOS
./build/P2POverlayNetwork 8888
```

### Interactive Menu

Once started, the application displays an interactive menu:

```
=== Main Menu ===
  1. Node Discovery
  2. Node Registration
  3. Dynamic Node Management
  4. Message Routing
  5. Reliable Messaging
  6. Data Exchange
  7. Exit
```

Each option opens a submenu with specific operations. For example:
- **Option 3 (Dynamic Node Management)**: Add/remove nodes, view network status
- **Option 4 (Message Routing)**: Send messages using different routing strategies
- **Option 6 (Data Exchange)**: Transfer large data files between nodes

## Configuration

### Network Constants

Defined in `include/Common.h`:

- `DEFAULT_PORT`: Default listening port (8888)
- `HEARTBEAT_INTERVAL_SEC`: Heartbeat interval in seconds (30)
- `NODE_TIMEOUT_SEC`: Node timeout threshold in seconds (90)
- `MAX_PEERS`: Maximum peer connections per node (10)

## Usage

### Starting a Node

The application accepts the following command-line arguments:

```
./P2POverlayNetwork <port> [bootstrap_host] [bootstrap_port]
```

- `port`: Local port to listen on
- `bootstrap_host`: Optional bootstrap node hostname
- `bootstrap_port`: Optional bootstrap node port

### Example Scenarios

1. **Starting the First Node**
   ```bash
   ./P2POverlayNetwork 8888
   ```

2. **Joining an Existing Network**
   ```bash
   ./P2POverlayNetwork 8889 localhost 8888
   ```

3. **Multiple Nodes**
   ```bash
   # Terminal 1
   ./P2POverlayNetwork 8888
   
   # Terminal 2
   ./P2POverlayNetwork 8889 localhost 8888
   
   # Terminal 3
   ./P2POverlayNetwork 8890 localhost 8888
   ```

## Advanced Features

The project includes several advanced components for enhanced functionality:

1. **Node Discovery** - Automatic peer discovery via bootstrap nodes
2. **Node Registration** - Secure node registration with validation
3. **Dynamic Node Management** - Self-healing network with failure detection
4. **Message Routing** - Smart routing with multiple strategies (shortest path, direct, flood)
5. **Reliable Messaging** - Guaranteed message delivery with acknowledgments
6. **Data Exchange** - Large data transfer support with chunking and progress tracking

### Interactive Menu System

The application features an interactive menu system for manual control:

- **Main Menu**: Access all features (1-7)
- **Submenus**: Each feature has its own submenu for detailed operations
- **Manual Control**: Create nodes, send messages, transfer data, and manage the network manually

## Project Structure

```
.
├── CMakeLists.txt          # CMake build configuration
├── README.md               # This file
├── .gitignore              # Git ignore rules
├── include/                # Header files
│   ├── Common.h           # Common types and constants
│   ├── Node.h             # Node class definition
│   ├── NetworkManager.h   # NetworkManager class definition
│   ├── TopologyManager.h  # TopologyManager class definition
│   ├── MessageHandler.h   # MessageHandler class definition
│   ├── NodeDiscovery.h    # Node discovery component
│   ├── NodeRegistration.h # Node registration component
│   ├── DynamicNodeManager.h # Dynamic node management
│   ├── MessageRouter.h    # Message routing component
│   ├── ReliableMessaging.h # Reliable messaging component
│   └── DataExchange.h     # Data exchange component
└── src/                    # Source files
    ├── main.cpp            # Application entry point with interactive menu
    ├── Node.cpp            # Node implementation
    ├── NetworkManager.cpp  # NetworkManager implementation
    ├── TopologyManager.cpp # TopologyManager implementation
    ├── MessageHandler.cpp  # MessageHandler implementation
    ├── NodeDiscovery.cpp  # Node discovery implementation
    ├── NodeRegistration.cpp # Node registration implementation
    ├── DynamicNodeManager.cpp # Dynamic node management implementation
    ├── MessageRouter.cpp   # Message routing implementation
    ├── ReliableMessaging.cpp # Reliable messaging implementation
    └── DataExchange.cpp    # Data exchange implementation
└── tests/                  # Test files
    ├── TestSuite.h
    └── TestSuite.cpp
```

## Design Decisions

### Thread Safety
All components use mutexes to ensure thread-safe operations, as the network operations may be accessed from multiple threads (server threads, client threads, main thread).

### Message Protocol
A simple binary protocol is used for efficiency. Messages include type, sender/receiver IDs, timestamp, and optional payload.

### Topology Representation
An adjacency list is used to represent the network graph, providing efficient neighbor queries and path finding.

### Connection Management
TCP connections are maintained for reliable communication. The system tracks active connections and handles reconnection scenarios.

## Future Enhancements

- Distributed hash table (DHT) for efficient node lookup
- Message routing with multiple hops
- Encryption and authentication
- NAT traversal support
- Performance metrics and monitoring
- Configuration file support
- Logging framework integration

## License

This project is provided as-is for educational and development purposes.

