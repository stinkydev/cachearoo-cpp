#ifndef CACHEAROO_TYPES_H_
#define CACHEAROO_TYPES_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cachearoo {

// Forward declarations
class CachearooClient;
class CachearooConnection;

// Constants
constexpr int kDefaultPort = 4300;
constexpr int kDefaultHttpPort = 80;
constexpr int kDefaultHttpsPort = 443;
constexpr int kMaxConcurrent = 50;
constexpr int kRequestTimeout = 3000;
constexpr int kDefaultPingInterval = 5000;

// Error constants for messaging
constexpr int kJobNotSupported = 100;
constexpr int kNoWorkerAvailable = 200;

// Exception classes
class TimeoutError : public std::exception {
 public:
  explicit TimeoutError(const std::string& message, bool progress_timeout = false)
      : message_(message), progress_timeout_(progress_timeout) {}

  const char* what() const noexcept override { return message_.c_str(); }
  bool IsProgressTimeout() const { return progress_timeout_; }

 private:
  std::string message_;
  bool progress_timeout_;
};

class AlreadyExistsError : public std::exception {
 public:
  explicit AlreadyExistsError(const std::string& message) : message_(message) {}
  const char* what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;
};

// Settings structure
struct CachearooSettings {
  std::string bucket;
  std::string host = "127.0.0.1";
  int port = kDefaultPort;
  std::string path;
  std::string api_key;
  bool secure = false;
  bool enable_ping = false;
  int ping_interval = kDefaultPingInterval;
  std::string client_id;
};

// Request options
struct RequestOptions {
  std::optional<std::string> bucket;
  std::optional<std::string> data;
  bool fail_if_exists = false;
  std::optional<std::string> expire;
  bool async = true;
  bool force_http = false;
  std::optional<bool> keys_only = false;
  std::optional<std::string> filter;
  bool remove_data_from_reply = false;
};

// List reply item
struct ListReplyItem {
  std::string key;
  std::string timestamp;
  int size;
  std::optional<std::string> expire;
  std::optional<std::string> content;
};

// Event structures
struct Event {
  std::string bucket;
  std::string key;
  bool is_deleted = false;
  std::optional<std::string> value;
};

struct BinaryEvent {
  std::string bucket;
  std::string key;
  int type;
  std::vector<uint8_t> content;
};

// Binary header structure (128 bytes fixed size)
struct BinaryHeader {
  uint8_t type;
  uint8_t bucket_size;
  uint8_t key_size;
  uint8_t api_key_size;
  std::string bucket;
  std::string key;
  std::string api_key;
};

// Callback types
using EventCallback = std::function<void(const Event&)>;
using BinaryEventCallback = std::function<void(const BinaryEvent&)>;
using ConnectCallback = std::function<void(CachearooConnection*)>;
using DisconnectCallback = std::function<void(CachearooConnection*)>;
using ErrorCallback = std::function<void(const std::string&)>;
using PongCallback = std::function<void()>;

// Messaging callback types
using MessageResponseCallback =
    std::function<void(const std::string& error, const std::string& data)>;
using ProgressCallback = std::function<void(const std::string& data)>;
using MessageCallback = std::function<void(
    const std::string& message, MessageResponseCallback response, ProgressCallback progress)>;

// Work callback types
using WorkCallback = std::function<void(const std::string& job, MessageResponseCallback callback,
                                        ProgressCallback progress_callback)>;
using JobQueryResponseCallback =
    std::function<void(int error_code, std::shared_ptr<class Worker> worker)>;
using JobQueryCallback =
    std::function<void(const std::string& job, JobQueryResponseCallback response)>;

// Utility function
std::string GenerateUuid();

}  // namespace cachearoo

#endif  // CACHEAROO_TYPES_H_