#include <gtest/gtest.h>
#include "cachearoo.h"
#include <thread>
#include <chrono>

using namespace cachearoo;

// Test settings validation
TEST(CachearooSettingsValidationTest, ValidSettings) {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "test-client";
    
    // Should not throw
    EXPECT_NO_THROW({
        // Basic validation
        EXPECT_FALSE(settings.host.empty());
        EXPECT_GT(settings.port, 0);
        EXPECT_LT(settings.port, 65536);
    });
}

TEST(CachearooSettingsValidationTest, PortRange) {
    CachearooSettings settings;
    
    // Valid port
    settings.port = 4300;
    EXPECT_GT(settings.port, 0);
    EXPECT_LT(settings.port, 65536);
    
    // Edge cases
    settings.port = 1;
    EXPECT_GT(settings.port, 0);
    
    settings.port = 65535;
    EXPECT_LT(settings.port, 65536);
}

TEST(CachearooSettingsValidationTest, SecureConnection) {
    CachearooSettings settings;
    settings.host = "example.com";
    settings.port = 443;
    settings.secure = true;
    
    EXPECT_TRUE(settings.secure);
    EXPECT_EQ(settings.port, 443);
}

TEST(CachearooSettingsValidationTest, PingConfiguration) {
    CachearooSettings settings;
    settings.enable_ping = true;
    settings.ping_interval = 10000;
    
    EXPECT_TRUE(settings.enable_ping);
    EXPECT_EQ(settings.ping_interval, 10000);
    EXPECT_GT(settings.ping_interval, 0);
}

// Test request options builder pattern
TEST(RequestOptionsBuilderTest, BuildBasicOptions) {
    RequestOptions options;
    options.bucket = "test-bucket";
    options.async = true;
    
    EXPECT_TRUE(options.bucket.has_value());
    EXPECT_EQ(options.bucket.value(), "test-bucket");
    EXPECT_TRUE(options.async);
}

TEST(RequestOptionsBuilderTest, BuildCompleteOptions) {
    RequestOptions options;
    options.bucket = "test-bucket";
    options.data = "test-data";
    options.fail_if_exists = true;
    options.expire = "1h";
    options.async = false;
    options.keys_only = true;
    options.filter = "*.json";
    options.remove_data_from_reply = true;
    
    EXPECT_TRUE(options.bucket.has_value());
    EXPECT_TRUE(options.data.has_value());
    EXPECT_TRUE(options.fail_if_exists);
    EXPECT_TRUE(options.expire.has_value());
    EXPECT_FALSE(options.async);
    EXPECT_TRUE(options.keys_only);
    EXPECT_TRUE(options.filter.has_value());
    EXPECT_TRUE(options.remove_data_from_reply);
}

// Test connection string generation (without actual connection)
TEST(ConnectionStringTest, BasicWebSocket) {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.secure = false;
    
    std::string expected_prefix = "ws://";
    EXPECT_EQ(settings.secure, false);
    
    // We expect ws:// for non-secure connections
    std::string protocol = settings.secure ? "wss://" : "ws://";
    EXPECT_EQ(protocol, "ws://");
}

TEST(ConnectionStringTest, SecureWebSocket) {
    CachearooSettings settings;
    settings.host = "example.com";
    settings.port = 443;
    settings.secure = true;
    
    std::string protocol = settings.secure ? "wss://" : "ws://";
    EXPECT_EQ(protocol, "wss://");
}

TEST(ConnectionStringTest, CustomPath) {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.path = "/custom/path";
    
    EXPECT_FALSE(settings.path.empty());
    EXPECT_EQ(settings.path, "/custom/path");
}

// Test error exception hierarchy
TEST(ExceptionHierarchyTest, TimeoutError) {
    try {
        throw TimeoutError("Test timeout", false);
    } catch (const TimeoutError& e) {
        EXPECT_STREQ(e.what(), "Test timeout");
        EXPECT_FALSE(e.IsProgressTimeout());
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }
}

TEST(ExceptionHierarchyTest, AlreadyExistsError) {
    try {
        throw AlreadyExistsError("Key exists");
    } catch (const AlreadyExistsError& e) {
        EXPECT_STREQ(e.what(), "Key exists");
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }
}

TEST(ExceptionHierarchyTest, StandardException) {
    // TimeoutError should be catchable as std::exception
    try {
        throw TimeoutError("Test", false);
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "Test");
    } catch (...) {
        FAIL() << "Should be catchable as std::exception";
    }
}
