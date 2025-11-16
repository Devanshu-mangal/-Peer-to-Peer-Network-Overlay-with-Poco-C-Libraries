cmake_minimum_required(VERSION 3.15)
project(P2POverlayNetwork VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Poco libraries
find_package(Poco REQUIRED COMPONENTS Foundation Net Util)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${Poco_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/Node.cpp
    src/NetworkManager.cpp
    src/TopologyManager.cpp
    src/MessageHandler.cpp
    src/NodeDiscovery.cpp
    src/NodeRegistration.cpp
    src/DynamicNodeManager.cpp
    src/NodeSimulator.cpp
    src/MessageRouter.cpp
    src/DataExchange.cpp
    src/ReliableMessaging.cpp
)

# Header files
set(HEADERS
    include/Node.h
    include/NetworkManager.h
    include/TopologyManager.h
    include/MessageHandler.h
    include/NodeDiscovery.h
    include/NodeRegistration.h
    include/DynamicNodeManager.h
    include/NodeSimulator.h
    include/MessageRouter.h
    include/DataExchange.h
    include/ReliableMessaging.h
    include/Common.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link Poco libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Poco::Foundation
    Poco::Net
    Poco::Util
)

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /EHsc)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

