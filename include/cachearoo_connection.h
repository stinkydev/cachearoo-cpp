#ifndef CACHEAROO_CONNECTION_H_
#define CACHEAROO_CONNECTION_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

// These macros are defined via CMake target_compile_definitions
// to avoid redefinition warnings
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#ifndef _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#endif
#ifndef _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#endif

#include <nlohmann/json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include "cachearoo_types.h"

namespace cachearoo {

// Forward declarations
struct RequestOptionsInternal;

class CachearooConnection {
 public:
  explicit CachearooConnection(const CachearooSettings& settings);
  ~CachearooConnection();

  // Connection management
  bool is_connected() const { return connected_.load(); }
  void close();
  const std::string& get_id() const { return id_; }

  // Event callbacks
  void set_on_connect(ConnectCallback callback) { on_connect_ = std::move(callback); }
  void set_on_disconnect(DisconnectCallback callback) { on_disconnect_ = std::move(callback); }
  void set_on_error(ErrorCallback callback) { on_error_ = std::move(callback); }
  void set_on_pong(PongCallback callback) { on_pong_ = std::move(callback); }

  // Event listeners
  int add_listener(const std::string& bucket, const std::string& key, bool send_values,
                   EventCallback callback);
  int add_binary_listener(const std::string& bucket, const std::string& key,
                          BinaryEventCallback callback);
  void remove_listener(int listener_id);
  void remove_binary_listener(int listener_id);
  void remove_all_listeners();

  // Data operations
  std::string read(const std::string& bucket, const std::string& key);
  std::vector<ListReplyItem> list(const std::string& bucket, bool keys_only,
                                  const std::string& filter);
  std::string write(const std::string& bucket, const std::string& key, const std::string& value,
                    bool fail_if_exists = false, const std::string& expire = "");
  std::string patch(const std::string& bucket, const std::string& key, const std::string& patch,
                    bool remove_data_from_reply = false);
  void delete_key(const std::string& bucket, const std::string& key);
  void signal_event(const std::string& bucket, const std::string& key, const std::string& value);
  void signal_binary_event(int type, const std::string& bucket, const std::string& key,
                           const std::vector<uint8_t>& value);

  // Generic request method
  std::string request(const RequestOptionsInternal& options);

 private:
  using WebSocketClient = websocketpp::client<websocketpp::config::asio_client>;
  using ConnectionHdl = websocketpp::connection_hdl;

  struct EventRegistration {
    int id;
    std::string bucket;
    std::string key;
    EventCallback callback;
  };

  struct BinaryEventRegistration {
    int id;
    std::string bucket;
    std::string key;
    BinaryEventCallback callback;
  };

  struct PendingRequest {
    std::string id;
    std::chrono::steady_clock::time_point timestamp;
    std::promise<std::string> promise;
  };

  // WebSocket event handlers
  void OnOpen(ConnectionHdl hdl);
  void OnClose(ConnectionHdl hdl);
  void OnFail(ConnectionHdl hdl);
  void OnMessage(ConnectionHdl hdl, WebSocketClient::message_ptr msg);

  // Message processing
  void ProcessStringMessage(const std::string& message);
  void ProcessBinaryMessage(const std::vector<uint8_t>& data);
  void HandleResponse(const nlohmann::json& response);

  // Helper methods
  void StartConnection();
  void RegisterEvents();
  void CheckRequestTimeouts();
  void AddRequest(const nlohmann::json& request, std::promise<std::string>& promise);
  std::vector<uint8_t> HeaderToBuffer(const BinaryHeader& header);
  BinaryHeader BufferToHeader(const std::vector<uint8_t>& buffer);

  // Settings and state
  CachearooSettings settings_;
  std::atomic<bool> connected_{false};
  std::atomic<bool> skip_reconnect_{false};
  std::string id_;
  bool send_values_{false};

  // WebSocket client
  std::unique_ptr<WebSocketClient> ws_client_;
  ConnectionHdl connection_hdl_;

  // Threading
  std::thread io_thread_;
  std::thread timeout_thread_;

  // Event handling
  std::vector<EventRegistration> event_registrations_;
  std::vector<BinaryEventRegistration> binary_event_registrations_;
  mutable std::mutex events_mutex_;
  int listener_id_counter_{0};

  // Request handling
  std::map<std::string, std::unique_ptr<PendingRequest>> pending_requests_;
  mutable std::mutex requests_mutex_;
  int request_id_counter_{0};

  // Callbacks
  ConnectCallback on_connect_;
  DisconnectCallback on_disconnect_;
  ErrorCallback on_error_;
  PongCallback on_pong_;

  // Ping handling
  std::thread ping_thread_;
  std::condition_variable ping_cv_;
  std::mutex ping_mutex_;

  // Pending message queue
  std::deque<std::string> pending_message_queue_;
  std::mutex pending_message_mutex_;
};

// Internal request options (extends RequestOptions)
struct RequestOptionsInternal : RequestOptions {
  std::string url;
  std::string method;
  std::string key;
};

}  // namespace cachearoo

#endif  // CACHEAROO_CONNECTION_H_