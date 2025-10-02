#include <iostream>
#include <thread>
#include <chrono>

#include "cachearoo.h"

using namespace cachearoo;

// Simple calculator service
void RunReplier() {
  try {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "calculator-replier";
    
    CachearooClient client(settings);
    
    // Wait for connection
    int attempts = 0;
    while (!client.IsConnected() && attempts < 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      attempts++;
    }
    
    if (!client.IsConnected()) {
      std::cerr << "Failed to connect to server" << std::endl;
      return;
    }
    
    std::cout << "Calculator service started!" << std::endl;
  
  Replier replier(&client, "calculator");
  
  replier.SetMessageHandler([](const std::string& message, 
                               MessageResponseCallback response,
                               ProgressCallback progress) {
    try {
      auto json_msg = nlohmann::json::parse(message);
      std::string operation = json_msg["operation"];
      double a = json_msg["a"];
      double b = json_msg["b"];
      std::string description = json_msg.value("description", "");
      
      std::cout << "Received calculation request: " << a << " " << operation << " " << b << std::endl;
      if (!description.empty()) {
        std::cout << "Description: " << description << std::endl;
      }
      
      // Simulate some processing time
      progress("Processing calculation...");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      
      double result = 0;
      if (operation == "add") {
        result = a + b;
      } else if (operation == "subtract") {
        result = a - b;
      } else if (operation == "multiply") {
        result = a * b;
      } else if (operation == "divide") {
        if (b == 0) {
          response("Division by zero", "");
          return;
        }
        result = a / b;
      } else {
        response("Unknown operation", "");
        return;
      }
      
      nlohmann::json result_json;
      result_json["result"] = result;
      if (!description.empty()) {
        result_json["echo"] = description;
      }
      response("", result_json.dump());
      
    } catch (const std::exception& e) {
      response(e.what(), "");
    }
  });
    
    std::cout << "Calculator service is running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    std::cout << "Shutting down server..." << std::endl;
    // Give time for any pending operations and close properly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client.Close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Server shutdown complete." << std::endl;
    
  } catch (const std::exception& e) {
    std::cerr << "Server error: " << e.what() << std::endl;
  }
}

void RunRequestor() {
  try {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "calculator-client";
    
    CachearooClient client(settings);
    
    // Wait for connection
    int attempts = 0;
    while (!client.IsConnected() && attempts < 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      attempts++;
    }
    
    if (!client.IsConnected()) {
      std::cerr << "Failed to connect to server" << std::endl;
      return;
    }
    
    std::cout << "Calculator client connected!" << std::endl;
  
  Requestor requestor(&client, "calculator");
  
  // Make some calculation requests
  try {
    // First request - simple addition
    nlohmann::json request;
    request["operation"] = "add";
    request["a"] = 10;
    request["b"] = 5;
    
    std::cout << "Requesting: 10 + 5" << std::endl;
    std::string result = requestor.Request(request.dump(), [](const std::string& progress) {
      std::cout << "Progress: " << progress << std::endl;
    });
    
    auto result_json = nlohmann::json::parse(result);
    std::cout << "Result: " << result_json["result"] << std::endl;
    
    // Second request - multiplication
    request["operation"] = "multiply";
    request["a"] = 7;
    request["b"] = 8;
    
    std::cout << "\nRequesting: 7 * 8" << std::endl;
    result = requestor.Request(request.dump());
    result_json = nlohmann::json::parse(result);
    std::cout << "Result: " << result_json["result"] << std::endl;
    
    // Third request - with Unicode characters (emojis, Chinese, Arabic, Cyrillic)
    request["operation"] = "divide";
    request["a"] = 100;
    request["b"] = 4;
    request["description"] = "Math operations: 数学 • الرياضيات • математика • 🧮✨";
    
    std::cout << "\nRequesting: 100 / 4 with Unicode description" << std::endl;
    result = requestor.Request(request.dump());
    result_json = nlohmann::json::parse(result);
    std::cout << "Result: " << result_json["result"] << std::endl;
    if (result_json.contains("echo")) {
      std::cout << "Echo: " << result_json["echo"] << std::endl;
      std::cout << "✓ Unicode characters transmitted successfully!" << std::endl;
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  
  std::cout << "Shutting down client..." << std::endl;
  // Give time for any pending operations and close properly
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  client.Close();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::cout << "Client shutdown complete." << std::endl;
  
  } catch (const std::exception& e) {
    std::cerr << "Client error: " << e.what() << std::endl;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " [server|client]" << std::endl;
    return 1;
  }
  
  std::string mode = argv[1];
  
  if (mode == "server") {
    RunReplier();
  } else if (mode == "client") {
    RunRequestor();
  } else {
    std::cout << "Invalid mode. Use 'server' or 'client'" << std::endl;
    return 1;
  }
  
  return 0;
}