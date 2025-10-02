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

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>

#include "cachearoo_types.h"

namespace cachearoo {

// Forward declarations
struct RequestOptionsInternal;

class CachearooConnection {
 public:
  explicit CachearooConnection(const CachearooSettings& settings);
  ~CachearooConnection();

  // Connection management
  bool IsConnected() const { return connected_.load(); }
  void Close();
  const std::string& GetId() const { return id_; }

  // Event callbacks
  void SetOnConnect(ConnectCallback callback) { on_connect_ = std::move(callback); }
  void SetOnDisconnect(DisconnectCallback callback) { on_disconnect_ = std::move(callback); }
  void SetOnError(ErrorCallback callback) { on_error_ = std::move(callback); }
  void SetOnPong(PongCallback callback) { on_pong_ = std::move(callback); }

  // Event listeners
  void AddListener(const std::string& bucket, const std::string& key, bool send_values,
                   EventCallback callback);
  void AddBinaryListener(const std::string& bucket, const std::string& key,
                         BinaryEventCallback callback);
  void RemoveListener(EventCallback callback);
  void RemoveBinaryListener(BinaryEventCallback callback);
  void RemoveAllListeners();

  // Data operations
  std::string Read(const std::string& bucket, const std::string& key);
  std::vector<ListReplyItem> List(const std::string& bucket, bool keys_only,
                                  const std::string& filter);
  std::string Write(const std::string& bucket, const std::string& key, const std::string& value,
                    bool fail_if_exists = false, const std::string& expire = "");
  std::string Patch(const std::string& bucket, const std::string& key, const std::string& patch,
                    bool remove_data_from_reply = false);
  void Delete(const std::string& bucket, const std::string& key);
  void SignalEvent(const std::string& bucket, const std::string& key, const std::string& value);
  void SignalBinaryEvent(int type, const std::string& bucket, const std::string& key,
                         const std::vector<uint8_t>& value);

  // Generic request method
  std::string Request(const RequestOptionsInternal& options);

 private:
  using WebSocketClient = websocketpp::client<websocketpp::config::asio_client>;
  using ConnectionHdl = websocketpp::connection_hdl;

  struct EventRegistration {
    std::string bucket;
    std::string key;
    EventCallback callback;
  };

  struct BinaryEventRegistration {
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