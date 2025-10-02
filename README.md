# Cachearoo C++ Library

A modern C++20 client library for communicating with Cachearoo server (a key/value store and event bus) via WebSocket connections.

## Features

- **WebSocket-only communication** - No HTTP interface, pure WebSocket using websocketpp
- **Modern C++20** - Uses latest C++ features including concepts, coroutines support ready
- **Google C++ Style** - Follows Google C++ style guide for naming and code structure
- **Header-only dependencies** - Uses nlohmann::json and websocketpp
- **Cross-platform** - Works on Windows, Linux, and macOS
- **Messaging patterns** - Built-in support for Request-Reply and Competing Consumers patterns
- **Event system** - Real-time event notifications with binary data support
- **Automatic reconnection** - Robust connection handling with automatic reconnection
- **Thread-safe** - All operations are thread-safe

## Building

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- Git (for downloading dependencies)

### Build Steps

```bash
# Clone the repository
git clone <repository-url>
cd cachearoo-cpp

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
cmake --build .

# Optionally build examples
cmake .. -DBUILD_EXAMPLES=ON
cmake --build .
```

#### Windows with Visual Studio

```powershell
# Create build directory
mkdir build
cd build

# Configure (generates .sln and .vcxproj files)
cmake ..

# Build with CMake
cmake --build . --config Debug

# Or open cachearoo-cpp.sln in Visual Studio and build from IDE
```

The build system will automatically download and configure the required dependencies:
- nlohmann/json (JSON handling)
- websocketpp (WebSocket client)
- standalone ASIO (async I/O library)

## Using in Your CMake Project

There are several ways to integrate this library into your own CMake project:

### Option 1: Add as Subdirectory

If you've cloned cachearoo-cpp into your project (e.g., in a `external/` or `libs/` directory):

```cmake
# In your CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(MyProject)

# Add cachearoo-cpp subdirectory
add_subdirectory(external/cachearoo-cpp)

# Create your executable
add_executable(my_app main.cpp)

# Link against cachearoo
target_link_libraries(my_app PRIVATE cachearoo)

# The include directories are automatically propagated
```

### Option 2: Install and Find Package

Build and install the library first:

```bash
cd cachearoo-cpp/build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local  # or your preferred install location
cmake --build . --config Release
cmake --install .
```

Then use `find_package` in your project:

```cmake
# In your CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(MyProject)

# Find cachearoo
find_package(cachearoo REQUIRED)

# Create your executable
add_executable(my_app main.cpp)

# Link against cachearoo
target_link_libraries(my_app PRIVATE cachearoo::cachearoo)
```

### Option 3: CMake FetchContent (Recommended)

Use CMake's `FetchContent` to automatically download and include the library:

```cmake
# In your CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(MyProject)

include(FetchContent)

# Fetch cachearoo-cpp
FetchContent_Declare(
    cachearoo
    GIT_REPOSITORY https://github.com/yourusername/cachearoo-cpp.git
    GIT_TAG main  # or a specific version tag
)

# Make cachearoo available
FetchContent_MakeAvailable(cachearoo)

# Create your executable
add_executable(my_app main.cpp)

# Link against cachearoo
target_link_libraries(my_app PRIVATE cachearoo)
```

### Minimal Example Project Structure

```
my_project/
├── CMakeLists.txt
├── main.cpp
└── external/
    └── cachearoo-cpp/  (if using Option 1)
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add cachearoo
add_subdirectory(external/cachearoo-cpp)

# Your application
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cachearoo)
```

**main.cpp:**
```cpp
#include "cachearoo.h"
#include <iostream>

int main() {
    cachearoo::CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "my-app";
    
    try {
        cachearoo::CachearooClient client(settings);
        
        // Wait for connection
        int attempts = 0;
        while (!client.IsConnected() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        if (!client.IsConnected()) {
            std::cerr << "Failed to connect to Cachearoo server\n";
            return 1;
        }
        
        std::cout << "Connected to Cachearoo!\n";
        
        // Use the client...
        client.Write("test", R"({"hello": "world"})");
        std::string data = client.Read("test");
        std::cout << "Data: " << data << "\n";
        
        client.Close();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

### What Gets Linked

When you link against `cachearoo`, your project automatically gets:
- The cachearoo static library (`.lib` on Windows, `.a` on Unix)
- All necessary include directories
- All required dependencies (nlohmann::json, websocketpp, ASIO)
- Proper C++20 compiler flags

No need to manually link against dependencies or set include paths - CMake handles everything automatically.

## Quick Start

### Basic Usage

```cpp
#include "cachearoo.h"

using namespace cachearoo;

int main() {
    // Configure connection
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.bucket = "my-bucket";
    settings.client_id = "my-client";
    
    // Create client
    CachearooClient client(settings);
    
    // Wait for connection
    while (!client.IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Write data
    std::string key = client.Write("test-key", R"({"message": "Hello World"})");
    
    // Read data
    std::string data = client.Read("test-key");
    
    // List keys
    auto items = client.List();
    
    // Update data
    client.Patch("test-key", R"({"message": "Updated message"})");
    
    // Remove data
    client.Remove("test-key");
    
    // Proper cleanup
    client.Close();
    
    return 0;
}
```

### Request-Reply Pattern

**Server (Replier):**
```cpp
#include "cachearoo.h"

using namespace cachearoo;

int main() {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "service-server";
    
    CachearooClient client(settings);
    
    // Wait for connection
    while (!client.IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Replier replier(&client, "calculator");
    
    replier.SetMessageHandler([](const std::string& message, 
                                 MessageResponseCallback response,
                                 ProgressCallback progress) {
        auto json_msg = nlohmann::json::parse(message);
        double a = json_msg["a"];
        double b = json_msg["b"];
        
        progress("Processing...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        nlohmann::json result;
        result["result"] = a + b;
        response("", result.dump());
    });
    
    std::cout << "Service running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    // Proper cleanup
    client.Close();
    
    return 0;
}
```

**Client (Requestor):**
```cpp
#include "cachearoo.h"

using namespace cachearoo;

int main() {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "service-client";
    
    CachearooClient client(settings);
    
    // Wait for connection
    while (!client.IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Requestor requestor(&client, "calculator");
    
    nlohmann::json request;
    request["a"] = 10;
    request["b"] = 5;
    
    std::string result = requestor.Request(request.dump(), [](const std::string& progress) {
        std::cout << "Progress: " << progress << std::endl;
    });
    
    auto result_json = nlohmann::json::parse(result);
    std::cout << "Result: " << result_json["result"] << std::endl;
    
    // Proper cleanup
    client.Close();
    
    return 0;
}
```

### Competing Consumers Pattern

**Consumer:**
```cpp
#include "cachearoo.h"

using namespace cachearoo;

class MyWorker : public Worker {
public:
    explicit MyWorker(const std::string& id) : Worker(id) {
        SetWorkHandler([this](const std::string& job, MessageResponseCallback callback, ProgressCallback progress) {
            ProcessJob(job, callback, progress);
        });
    }
    
private:
    void ProcessJob(const std::string& job, MessageResponseCallback callback, ProgressCallback progress) {
        progress("Starting job...");
        
        // Do work here
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        progress("Job almost done...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        callback("", "Job completed successfully");
    }
};

int main() {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "worker-consumer";
    
    CachearooClient client(settings);
    
    // Wait for connection
    while (!client.IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    CompetingConsumer consumer(&client, "work-queue", settings.client_id);
    
    // Create workers
    std::vector<std::shared_ptr<MyWorker>> workers;
    for (int i = 1; i <= 3; ++i) {
        workers.push_back(std::make_shared<MyWorker>("worker-" + std::to_string(i)));
    }
    
    // Set up job query handler
    consumer.SetJobQueryHandler([&workers](const std::string& job, JobQueryResponseCallback response) {
        // Find available worker
        for (auto& worker : workers) {
            if (worker->IsAvailable()) {
                worker->SetAvailable(false);
                response(0, worker);
                return;
            }
        }
        response(kNoWorkerAvailable, nullptr);
    });
    
    std::cout << "Consumer running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    // Proper cleanup
    client.Close();
    
    return 0;
}
```

**Producer:**
```cpp
#include "cachearoo.h"

using namespace cachearoo;

int main() {
    CachearooSettings settings;
    settings.host = "localhost";
    settings.port = 4300;
    settings.client_id = "job-producer";
    
    CachearooClient client(settings);
    
    // Wait for connection
    while (!client.IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Producer producer(&client, "work-queue");
    
    // Submit jobs
    for (int i = 1; i <= 5; ++i) {
        nlohmann::json job;
        job["id"] = "job-" + std::to_string(i);
        job["data"] = "Work item " + std::to_string(i);
        
        try {
            std::string result = producer.AddJob(job.dump(), [i](const std::string& progress) {
                std::cout << "Job " << i << " progress: " << progress << std::endl;
            });
            
            std::cout << "Job " << i << " completed: " << result << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Job " << i << " failed: " << e.what() << std::endl;
        }
    }
    
    // Proper cleanup
    client.Close();
    
    return 0;
}
```

## API Reference

### CachearooSettings

Configuration structure for the client:

```cpp
struct CachearooSettings {
    std::string bucket;           // Default bucket name
    std::string host = "127.0.0.1"; // Server hostname
    int port = 4300;              // Server port
    std::string path;             // URL path prefix
    std::string api_key;          // API key for authentication
    bool secure = false;          // Use WSS instead of WS
    bool enable_ping = false;     // Enable keep-alive pings
    int ping_interval = 5000;     // Ping interval in milliseconds
    std::string client_id;        // Unique client identifier
};
```

### CachearooClient

Main client class for data operations:

#### Data Operations
- `std::string Read(const std::string& key, const RequestOptions& options = {})`
- `std::vector<ListReplyItem> List(const RequestOptions& options = {})`
- `std::string Write(const std::string& key, const std::string& value, const RequestOptions& options = {})`
- `std::string Patch(const std::string& key, const std::string& patch, const RequestOptions& options = {})`
- `void Remove(const std::string& key, const RequestOptions& options = {})`

#### Connection Management
- `bool IsConnected() const`
- `void Close()`
- `CachearooConnection* GetConnection()`

### RequestOptions

Options for data operations:

```cpp
struct RequestOptions {
    std::optional<std::string> bucket;        // Override default bucket
    std::optional<std::string> data;          // Data for write operations
    bool fail_if_exists = false;              // Fail if key already exists
    std::optional<std::string> expire;        // Expiration time
    bool async = true;                        // Use async operations
    bool keys_only = false;                   // Return only keys in list operations
    std::optional<std::string> filter;        // Filter for list operations
    bool remove_data_from_reply = false;      // Don't return data in response
};
```

**Note:** The `force_http` option is not supported as this library is WebSocket-only.

### Event System

Subscribe to real-time events:

```cpp
// Text events
client.GetConnection()->AddListener("bucket", "key", true, [](const Event& event) {
    std::cout << "Event: " << event.key << " = " << event.value.value_or("") << std::endl;
});

// Binary events
client.GetConnection()->AddBinaryListener("bucket", "key", [](const BinaryEvent& event) {
    std::cout << "Binary event: " << event.key << ", type: " << event.type 
              << ", size: " << event.content.size() << std::endl;
});
```

### Error Handling

The library throws standard C++ exceptions:

- `std::runtime_error` - General runtime errors
- `TimeoutError` - Request timeouts (inherits from std::exception)
- `AlreadyExistsError` - Key already exists errors (inherits from std::exception)

```cpp
try {
    client.Write("existing-key", "data", options);
} catch (const AlreadyExistsError& e) {
    std::cout << "Key already exists: " << e.what() << std::endl;
} catch (const TimeoutError& e) {
    std::cout << "Timeout: " << e.what() << std::endl;
    if (e.IsProgressTimeout()) {
        std::cout << "This was a progress timeout" << std::endl;
    }
} catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << std::endl;
}
```

## Examples

The `examples/` directory contains complete working examples:

- `basic_example` - Basic CRUD operations
- `request_reply_example` - Calculator service using request-reply pattern
- `competing_consumers_example` - Image processing service using competing consumers

To build and run examples:

```bash
# Build with examples
cmake .. -DBUILD_EXAMPLES=ON
cmake --build . --config Debug

# Run basic example (Linux/macOS)
./examples/basic_example

# Run basic example (Windows)
.\examples\Debug\basic_example.exe

# Run request-reply example (start server first)
# Terminal 1 - Server
./examples/request_reply_example server        # Linux/macOS
.\examples\Debug\request_reply_example.exe server  # Windows

# Terminal 2 - Client
./examples/request_reply_example client        # Linux/macOS
.\examples\Debug\request_reply_example.exe client  # Windows

# Run competing consumers example (start consumer first)
# Terminal 1 - Consumer
./examples/competing_consumers_example consumer        # Linux/macOS
.\examples\Debug\competing_consumers_example.exe consumer  # Windows

# Terminal 2 - Producer
./examples/competing_consumers_example producer        # Linux/macOS
.\examples\Debug\competing_consumers_example.exe producer  # Windows
```

## Thread Safety

All operations are thread-safe. The library uses internal mutexes and atomic operations to ensure thread safety across:

- Multiple concurrent data operations
- Event callbacks executing in parallel
- Connection state management
- Request queue processing

## Performance Considerations

- WebSocket-only communication provides consistent low-latency performance
- Enable connection pooling for high-throughput scenarios
- Consider using async operations for better concurrency
- Use binary events for large data transfers
- Always call `Close()` before destroying the client to ensure proper cleanup

## Best Practices

### Proper Cleanup

Always call `Close()` on the client before it goes out of scope to ensure proper cleanup of connections and threads:

```cpp
{
    CachearooClient client(settings);
    
    // ... use the client ...
    
    client.Close();  // Important: Clean shutdown
}  // Client destructor is now safe
```

### Error Handling

Wrap client operations in try-catch blocks:

```cpp
try {
    CachearooClient client(settings);
    
    // Wait for connection with timeout
    int attempts = 0;
    while (!client.IsConnected() && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }
    
    if (!client.IsConnected()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Perform operations
    client.Write("key", "value");
    
    client.Close();
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
}
```

## License

[License information here]

## Troubleshooting

### Build Issues

**Issue: CMake can't find dependencies**
- Make sure you have an internet connection during first build (dependencies are downloaded)
- Delete the `build/` directory and rebuild from scratch
- Check that Git is installed and accessible from command line

**Issue: Compilation errors with ASIO or WebSocket++**
- Ensure you're using C++20 compatible compiler
- On Windows: Use Visual Studio 2019 or newer
- On Linux: Use GCC 10+ or Clang 11+

### Runtime Issues

**Issue: "Failed to connect" errors**
- Verify Cachearoo server is running on specified host:port
- Check firewall settings
- Ensure client_id is unique across all clients
- Add connection timeout logic:
  ```cpp
  int attempts = 0;
  while (!client.IsConnected() && attempts < 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      attempts++;
  }
  if (!client.IsConnected()) {
      throw std::runtime_error("Connection timeout");
  }
  ```

**Issue: Request timeout errors**
- Default timeout is 30 seconds
- Check server is responding
- For long-running operations, use progress callbacks to prevent timeouts
- Server must send progress updates for operations taking >30 seconds

**Issue: WebSocket connection drops unexpectedly**
- Enable ping/keep-alive: `settings.enable_ping = true;`
- Library automatically reconnects on connection loss
- Implement connection state monitoring in your application

### Performance Issues

**Issue: High latency**
- WebSocket connection provides low-latency by default
- Check network conditions between client and server
- Consider geographic proximity of client and server
- Use binary events for large data transfers

**Issue: Memory leaks**
- Always call `Close()` before destroying client
- Don't forget to clear event listeners when no longer needed
- Use RAII patterns to ensure proper cleanup

## FAQ

**Q: Does this library support HTTP requests?**  
A: No, this is a WebSocket-only implementation. All communication happens over WebSocket connections.

**Q: Can I use this library with Boost.Asio?**  
A: This library uses standalone ASIO (no Boost dependency). It's designed to be lightweight and portable.

**Q: Is the library thread-safe?**  
A: Yes, all operations are thread-safe using internal mutexes and atomic operations.

**Q: What happens if the connection drops?**  
A: The library automatically attempts to reconnect. You can monitor connection state with `IsConnected()`.

**Q: Can I use this with vcpkg or Conan?**  
A: The library uses CMake ExternalProject to manage dependencies automatically. You don't need vcpkg or Conan.

**Q: Does it work on embedded systems?**  
A: It should work on any platform with C++20 support, but is primarily tested on Windows, Linux, and macOS.

## Contributing

[Contributing guidelines here]