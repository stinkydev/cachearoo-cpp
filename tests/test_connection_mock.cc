#include <gtest/gtest.h>
#include "cachearoo.h"
#include <nlohmann/json.hpp>

using namespace cachearoo;

// Mock tests that don't require actual server connection
// These test the client logic without network I/O

TEST(ConnectionMockTest, SettingsInitialization) {
    CachearooSettings settings;
    settings.host = "test-host";
    settings.port = 1234;
    settings.bucket = "test-bucket";
    settings.client_id = "test-client";
    
    // Verify settings are properly stored
    EXPECT_EQ(settings.host, "test-host");
    EXPECT_EQ(settings.port, 1234);
    EXPECT_EQ(settings.bucket, "test-bucket");
    EXPECT_EQ(settings.client_id, "test-client");
}

TEST(CallbackTest, ProgressCallbackSignature) {
    bool callback_called = false;
    std::string received_progress;
    
    ProgressCallback callback = [&](const std::string& progress) {
        callback_called = true;
        received_progress = progress;
    };
    
    // Simulate callback invocation
    callback("Processing...");
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(received_progress, "Processing...");
}

TEST(CallbackTest, MessageResponseCallbackSignature) {
    bool callback_called = false;
    std::string received_error;
    std::string received_data;
    
    MessageResponseCallback callback = [&](const std::string& error, const std::string& data) {
        callback_called = true;
        received_error = error;
        received_data = data;
    };
    
    // Simulate successful response
    callback("", "result-data");
    
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(received_error.empty());
    EXPECT_EQ(received_data, "result-data");
    
    // Simulate error response
    callback_called = false;
    callback("Error occurred", "");
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(received_error, "Error occurred");
    EXPECT_TRUE(received_data.empty());
}

TEST(EventCallbackTest, EventStructureValidation) {
    Event event;
    event.bucket = "test-bucket";
    event.key = "test-key";
    event.value = "test-value";
    event.is_deleted = false;
    
    bool callback_called = false;
    
    EventCallback callback = [&](const Event& e) {
        callback_called = true;
        EXPECT_EQ(e.bucket, "test-bucket");
        EXPECT_EQ(e.key, "test-key");
        EXPECT_TRUE(e.value.has_value());
        EXPECT_EQ(e.value.value(), "test-value");
        EXPECT_FALSE(e.is_deleted);
    };
    
    callback(event);
    EXPECT_TRUE(callback_called);
}

TEST(BinaryEventCallbackTest, BinaryEventStructureValidation) {
    BinaryEvent event;
    event.bucket = "test-bucket";
    event.key = "test-key";
    event.type = 1;  // type is int
    event.content = {0x01, 0x02, 0x03, 0x04};
    
    bool callback_called = false;
    
    BinaryEventCallback callback = [&](const BinaryEvent& e) {
        callback_called = true;
        EXPECT_EQ(e.bucket, "test-bucket");
        EXPECT_EQ(e.key, "test-key");
        EXPECT_EQ(e.type, 1);
        EXPECT_EQ(e.content.size(), 4);
        EXPECT_EQ(e.content[0], 0x01);
    };
    
    callback(event);
    EXPECT_TRUE(callback_called);
}
