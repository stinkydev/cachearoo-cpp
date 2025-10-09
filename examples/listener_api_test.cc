#include <iostream>
#include "cachearoo.h"

using namespace cachearoo;

int main() {
    CachearooSettings settings;
    settings.host = "127.0.0.1";
    settings.port = 9080;
    
    CachearooClient client(settings);
    
    // Demonstrate the new listener API
    std::cout << "Testing new AddListener API that returns IDs...\n";
    
    // Add a listener and store the ID
    int listener_id = client.GetConnection()->AddListener("test-bucket", "*", true, 
        [&client](const Event& event) {
            std::cout << "Event received: " << event.key << std::endl;
            
            // This is now safe - no deadlock when calling List from within callback!
            try {
                auto items = client.List();
                std::cout << "List call from callback succeeded - found " << items.size() << " items\n";
            } catch (const std::exception& e) {
                std::cout << "List call failed: " << e.what() << "\n";
            }
        });
    
    std::cout << "Added listener with ID: " << listener_id << std::endl;
    
    // Add another listener
    int listener_id2 = client.GetConnection()->AddListener("test-bucket", "specific-key", true,
        [](const Event& event) {
            std::cout << "Specific key event: " << event.key << std::endl;
        });
    
    std::cout << "Added second listener with ID: " << listener_id2 << std::endl;
    
    // Remove the first listener by ID
    client.GetConnection()->RemoveListener(listener_id);
    std::cout << "Removed listener " << listener_id << std::endl;
    
    // Remove the second listener by ID  
    client.GetConnection()->RemoveListener(listener_id2);
    std::cout << "Removed listener " << listener_id2 << std::endl;
    
    std::cout << "Test completed successfully!\n";
    
    return 0;
}