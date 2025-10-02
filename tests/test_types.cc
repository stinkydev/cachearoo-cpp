#include <gtest/gtest.h>
#include "cachearoo_types.h"

using namespace cachearoo;

// Test CachearooSettings default values
TEST(CachearooSettingsTest, DefaultValues) {
    CachearooSettings settings;
    
    EXPECT_EQ(settings.host, "127.0.0.1");
    EXPECT_EQ(settings.port, 4300);
    EXPECT_FALSE(settings.secure);
    EXPECT_FALSE(settings.enable_ping);
    EXPECT_EQ(settings.ping_interval, 5000);
    EXPECT_TRUE(settings.bucket.empty());
    EXPECT_TRUE(settings.path.empty());
    EXPECT_TRUE(settings.api_key.empty());
    EXPECT_TRUE(settings.client_id.empty());
}

// Test CachearooSettings custom values
TEST(CachearooSettingsTest, CustomValues) {
    CachearooSettings settings;
    settings.host = "example.com";
    settings.port = 8080;
    settings.bucket = "test-bucket";
    settings.secure = true;
    settings.enable_ping = true;
    settings.client_id = "test-client";
    settings.api_key = "secret-key";
    
    EXPECT_EQ(settings.host, "example.com");
    EXPECT_EQ(settings.port, 8080);
    EXPECT_EQ(settings.bucket, "test-bucket");
    EXPECT_TRUE(settings.secure);
    EXPECT_TRUE(settings.enable_ping);
    EXPECT_EQ(settings.client_id, "test-client");
    EXPECT_EQ(settings.api_key, "secret-key");
}

// Test RequestOptions default values
TEST(RequestOptionsTest, DefaultValues) {
    RequestOptions options;
    
    EXPECT_FALSE(options.bucket.has_value());
    EXPECT_FALSE(options.data.has_value());
    EXPECT_FALSE(options.fail_if_exists);
    EXPECT_FALSE(options.expire.has_value());
    EXPECT_TRUE(options.async);
    EXPECT_FALSE(options.keys_only);
    EXPECT_FALSE(options.filter.has_value());
    EXPECT_FALSE(options.remove_data_from_reply);
}

// Test RequestOptions custom values
TEST(RequestOptionsTest, CustomValues) {
    RequestOptions options;
    options.bucket = "my-bucket";
    options.data = "test-data";
    options.fail_if_exists = true;
    options.expire = "1h";
    options.async = false;
    options.keys_only = true;
    options.filter = "*.json";
    options.remove_data_from_reply = true;
    
    EXPECT_TRUE(options.bucket.has_value());
    EXPECT_EQ(options.bucket.value(), "my-bucket");
    EXPECT_TRUE(options.data.has_value());
    EXPECT_EQ(options.data.value(), "test-data");
    EXPECT_TRUE(options.fail_if_exists);
    EXPECT_TRUE(options.expire.has_value());
    EXPECT_EQ(options.expire.value(), "1h");
    EXPECT_FALSE(options.async);
    EXPECT_TRUE(options.keys_only);
    EXPECT_TRUE(options.filter.has_value());
    EXPECT_EQ(options.filter.value(), "*.json");
    EXPECT_TRUE(options.remove_data_from_reply);
}

// Test Event structure
TEST(EventTest, Construction) {
    Event event;
    event.bucket = "test-bucket";
    event.key = "test-key";
    event.value = "test-value";
    event.is_deleted = false;
    
    EXPECT_EQ(event.bucket, "test-bucket");
    EXPECT_EQ(event.key, "test-key");
    EXPECT_TRUE(event.value.has_value());
    EXPECT_EQ(event.value.value(), "test-value");
    EXPECT_FALSE(event.is_deleted);
}

// Test BinaryEvent structure
TEST(BinaryEventTest, Construction) {
    BinaryEvent event;
    event.bucket = "test-bucket";
    event.key = "test-key";
    event.type = 1;  // type is int, not string
    event.content = {0x89, 0x50, 0x4E, 0x47}; // PNG header
    
    EXPECT_EQ(event.bucket, "test-bucket");
    EXPECT_EQ(event.key, "test-key");
    EXPECT_EQ(event.type, 1);
    EXPECT_EQ(event.content.size(), 4);
    EXPECT_EQ(event.content[0], 0x89);
}

// Test ListReplyItem structure
TEST(ListReplyItemTest, Construction) {
    ListReplyItem item;
    item.key = "my-key";
    item.timestamp = "2025-10-03T12:00:00Z";
    item.size = 1024;
    item.expire = "1h";
    item.content = "my-value";
    
    EXPECT_EQ(item.key, "my-key");
    EXPECT_EQ(item.timestamp, "2025-10-03T12:00:00Z");
    EXPECT_EQ(item.size, 1024);
    EXPECT_TRUE(item.expire.has_value());
    EXPECT_EQ(item.expire.value(), "1h");
    EXPECT_TRUE(item.content.has_value());
    EXPECT_EQ(item.content.value(), "my-value");
}

// Test TimeoutError exception
TEST(TimeoutErrorTest, BasicUsage) {
    TimeoutError error("Request timed out", false);
    
    EXPECT_STREQ(error.what(), "Request timed out");
    EXPECT_FALSE(error.IsProgressTimeout());
}

TEST(TimeoutErrorTest, ProgressTimeout) {
    TimeoutError error("Progress timeout", true);
    
    EXPECT_STREQ(error.what(), "Progress timeout");
    EXPECT_TRUE(error.IsProgressTimeout());
}

// Test AlreadyExistsError exception
TEST(AlreadyExistsErrorTest, BasicUsage) {
    AlreadyExistsError error("Key already exists");
    
    EXPECT_STREQ(error.what(), "Key already exists");
}
