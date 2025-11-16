#ifndef TEST_SUITE_H
#define TEST_SUITE_H

#include "../include/NodeSimulator.h"
#include <string>
#include <vector>
#include <functional>

namespace P2POverlay {

/**
 * Test result
 */
struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
    double durationSeconds;
    
    TestResult() : passed(false), durationSeconds(0.0) {}
};

/**
 * Comprehensive test suite for P2P overlay
 */
class TestSuite {
public:
    TestSuite();
    ~TestSuite();
    
    // Test execution
    std::vector<TestResult> runAllTests();
    bool runTest(const std::string& testName);
    
    // Individual tests
    TestResult testNodeDiscovery();
    TestResult testNodeRegistration();
    TestResult testDynamicNodeAddition();
    TestResult testNodeRemoval();
    TestResult testNodeFailure();
    TestResult testNetworkIntegrity();
    TestResult testMessageRouting();
    TestResult testReliableMessaging();
    TestResult testDataExchange();
    TestResult testMultiHopRouting();
    TestResult testNetworkScalability();
    TestResult testConcurrentOperations();
    
    // Test utilities
    void setupTestNetwork(size_t nodeCount);
    void teardownTestNetwork();
    
    // Statistics
    size_t getTotalTests() const { return totalTests_; }
    size_t getPassedTests() const { return passedTests_; }
    size_t getFailedTests() const { return failedTests_; }
    double getTotalDuration() const { return totalDuration_; }
    
private:
    NetworkSimulator* simulator_;
    std::vector<TestResult> testResults_;
    size_t totalTests_;
    size_t passedTests_;
    size_t failedTests_;
    double totalDuration_;
    
    // Helper methods
    bool waitForCondition(std::function<bool()> condition, int timeoutSeconds = 10);
    void logTestResult(const TestResult& result);
};

} // namespace P2POverlay

#endif // TEST_SUITE_H

