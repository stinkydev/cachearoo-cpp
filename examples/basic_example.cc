#include <chrono>
#include <iostream>
#include <thread>

#include "cachearoo.h"

using namespace cachearoo;

int main() {
  try {
    // Configure settings
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.bucket = "test-bucket";
    settings.enable_ping = true;
    settings.client_id = "basic-example-client";

    std::cout << "Creating Cachearoo client..." << std::endl;
    CachearooClient client(settings);

    // Wait for connection
    std::cout << "Waiting for connection..." << std::endl;
    while (!client.is_connected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Connected!" << std::endl;

    try {
      // Write some data
      std::cout << "Writing data..." << std::endl;
      std::string key = client.write("test-key", R"({"message": "Hello, Cachearoo!"})");
      std::cout << "Written to key: " << key << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Write failed: " << e.what() << std::endl;
    }

    try {
      // Read the data back
      std::cout << "Reading data..." << std::endl;
      std::string data = client.read("test-key");
      std::cout << "Read data: " << data << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Read failed: " << e.what() << std::endl;
    }

    try {
      // List all keys
      std::cout << "Listing keys..." << std::endl;
      RequestOptions list_options;
      list_options.keys_only = true;
      auto items = client.list(list_options);

      std::cout << "Found " << items.size() << " items:" << std::endl;
      for (const auto& item : items) {
        std::cout << "  Key: " << item.key << ", Size: " << item.size << std::endl;
      }
    } catch (const std::exception& e) {
      std::cerr << "List failed: " << e.what() << std::endl;
    }

    try {
      // Update the data using patch
      std::cout << "Patching data..." << std::endl;
      client.patch("test-key", R"({"message": "Hello, Updated Cachearoo!"})");

      // Read the updated data
      std::string updated_data = client.read("test-key");
      std::cout << "Updated data: " << updated_data << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Patch/Read failed: " << e.what() << std::endl;
    }

    try {
      // Clean up
      std::cout << "Removing data..." << std::endl;
      client.remove("test-key");
    } catch (const std::exception& e) {
      std::cerr << "Remove failed: " << e.what() << std::endl;
    }

    std::cout << "Basic example completed successfully!" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
