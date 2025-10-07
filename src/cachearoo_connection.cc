#include "cachearoo_connection.h"

#include <chrono>
#include <cstring>
#include <random>
#include <set>
#include <sstream>
#include <algorithm>
#include <deque>

#include <nlohmann/json.hpp>

namespace cachearoo {

std::string GenerateUuid() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);
  static std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  int i;
  ss << std::hex;
  for (i = 0; i < 8; i++) {
    ss << dis(gen);
  }
  ss << "-";
  for (i = 0; i < 4; i++) {
    ss << dis(gen);
  }
  ss << "-4";
  for (i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  ss << dis2(gen);
  for (i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  for (i = 0; i < 12; i++) {
    ss << dis(gen);
  };
  return ss.str();
}

CachearooConnection::CachearooConnection(const CachearooSettings& settings) : settings_(settings) {
  // For now, only support non-secure WebSocket
  if (settings_.secure) {
    throw std::runtime_error("Secure WebSocket (WSS) not supported in this build");
  }

  ws_client_ = std::make_unique<WebSocketClient>();

  // Configure WebSocket client ONCE
  ws_client_->clear_access_channels(websocketpp::log::alevel::all);
  ws_client_->clear_error_channels(websocketpp::log::elevel::all);
  ws_client_->init_asio();
  ws_client_->set_reuse_addr(true);

  // Set handlers
  ws_client_->set_open_handler([this](ConnectionHdl hdl) { OnOpen(hdl); });
  ws_client_->set_close_handler([this](ConnectionHdl hdl) { OnClose(hdl); });
  ws_client_->set_fail_handler([this](ConnectionHdl hdl) { OnFail(hdl); });
  ws_client_->set_message_handler(
      [this](ConnectionHdl hdl, WebSocketClient::message_ptr msg) { OnMessage(hdl, msg); });

  StartConnection();

  // Start timeout checking thread
  timeout_thread_ = std::thread([this]() { CheckRequestTimeouts(); });

  // Start ping thread if enabled
  if (settings_.enable_ping) {
    ping_thread_ = std::thread([this]() {
      std::unique_lock<std::mutex> lock(ping_mutex_);
      while (!skip_reconnect_.load()) {
        ping_cv_.wait_for(lock, std::chrono::milliseconds(settings_.ping_interval));
        if (!skip_reconnect_.load() && connected_.load()) {
          nlohmann::json ping_msg;
          ping_msg["msg"] = "ping";

          if (ws_client_) {
            ws_client_->send(connection_hdl_, ping_msg.dump(), websocketpp::frame::opcode::text);
          }
        }
      }
    });
  }
}

CachearooConnection::~CachearooConnection() {
  Close();
}

void CachearooConnection::Close() {
  skip_reconnect_.store(true);
  connected_.store(false);

  // Notify ping thread to stop
  if (ping_thread_.joinable()) {
    ping_cv_.notify_all();
    ping_thread_.join();
  }

  // Close WebSocket connection
  if (ws_client_) {
    try {
      ws_client_->close(connection_hdl_, websocketpp::close::status::going_away, "");
    } catch (...) {
      // Ignore close errors
    }
    // Stop the ASIO event loop to unblock io_thread_
    ws_client_->stop();
  }

  // Join threads
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
  if (timeout_thread_.joinable()) {
    timeout_thread_.join();
  }

  // Clear all pending requests
  std::lock_guard<std::mutex> lock(requests_mutex_);
  for (auto& [id, request] : pending_requests_) {
    request->promise.set_exception(
        std::make_exception_ptr(std::runtime_error("Connection closed")));
  }
  pending_requests_.clear();
}

void CachearooConnection::StartConnection() {
  skip_reconnect_.store(false);
  connected_.store(false);

  std::string protocol = "ws://";  // Only support non-secure for now
  std::string url = protocol + settings_.host + ":" + std::to_string(settings_.port) +
                    settings_.path + "?id=" + settings_.client_id;

  // Create connection
  websocketpp::lib::error_code ec;
  auto con = ws_client_->get_connection(url, ec);
  if (ec) {
    if (on_error_)
      on_error_("Failed to create connection: " + ec.message());
    return;
  }

  connection_hdl_ = con->get_handle();
  ws_client_->connect(con);

  // Start IO thread (only if not already running)
  if (!io_thread_.joinable()) {
    io_thread_ = std::thread([this]() { ws_client_->run(); });
  }
}

void CachearooConnection::OnOpen(ConnectionHdl hdl) {
  connection_hdl_ = hdl;

  // Give the transport layer a moment to fully initialize
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  connected_.store(true);

  // Register events with lock
  {
    std::lock_guard<std::mutex> lock(events_mutex_);
    RegisterEvents();
  }

  {
    std::lock_guard<std::mutex> lock(pending_message_mutex_);
    while (!pending_message_queue_.empty()) {
      try {
        ws_client_->send(connection_hdl_, pending_message_queue_.front(),
                         websocketpp::frame::opcode::text);
      } catch (const std::exception& e) {
        if (on_error_)
          on_error_(std::string("WebSocket send failed (OnOpen): ") + e.what());
      }
      pending_message_queue_.pop_front();
    }
  }
}

void CachearooConnection::OnClose(ConnectionHdl /* hdl */) {
  if (connected_.load()) {
    connected_.store(false);
    if (on_disconnect_)
      on_disconnect_(this);
  }

  // Reconnect if not explicitly closed
  if (!skip_reconnect_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    if (!skip_reconnect_.load()) {
      StartConnection();
    }
  }
}

void CachearooConnection::OnFail(ConnectionHdl /* hdl */) {
  if (on_error_)
    on_error_("WebSocket connection failed");

  // Reconnect if not explicitly closed
  if (!skip_reconnect_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    if (!skip_reconnect_.load()) {
      StartConnection();
    }
  }
}

void CachearooConnection::OnMessage(ConnectionHdl /* hdl */, WebSocketClient::message_ptr msg) {
  if (msg->get_opcode() == websocketpp::frame::opcode::text) {
    ProcessStringMessage(msg->get_payload());
  } else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
    const std::string& payload = msg->get_payload();
    std::vector<uint8_t> data(payload.begin(), payload.end());
    ProcessBinaryMessage(data);
  }
}

void CachearooConnection::ProcessStringMessage(const std::string& message) {
  try {
    auto json_msg = nlohmann::json::parse(message);

    // Handle connection ID
    if (json_msg.contains("id")) {
      id_ = json_msg["id"].get<std::string>();
      if (on_connect_)
        on_connect_(this);
      return;
    }

    // Handle response
    if (json_msg.contains("response")) {
      HandleResponse(json_msg["response"]);
      return;
    }

    // Handle pong
    if (json_msg.contains("pong")) {
      if (on_pong_)
        on_pong_();
      return;
    }

    // Handle event
    Event event;
    event.bucket = json_msg.value("bucket", "");
    event.key = json_msg.value("key", "");
    event.is_deleted = json_msg.value("isDeleted", false);

    if (json_msg.contains("value") && !json_msg.contains("isDeleted")) {
      event.value = json_msg["value"].dump();
    }

    // Fire event callbacks
    std::lock_guard<std::mutex> lock(events_mutex_);
    for (const auto& reg : event_registrations_) {
      if ((reg.bucket == "*" || reg.bucket == event.bucket) &&
          (reg.key == "*" || reg.key == event.key)) {
        reg.callback(event);
      }
    }
  } catch (const std::exception& e) {
    if (on_error_)
      on_error_("Failed to process message: " + std::string(e.what()));
  }
}

void CachearooConnection::ProcessBinaryMessage(const std::vector<uint8_t>& data) {
  if (data.size() < 128) {
    if (on_error_)
      on_error_("Binary data without header");
    return;
  }

  BinaryHeader header = BufferToHeader(data);

  BinaryEvent event;
  event.type = header.type;
  event.bucket = header.bucket;
  event.key = header.key;
  event.content = std::vector<uint8_t>(data.begin() + 128, data.end());

  // Fire binary event callbacks
  std::lock_guard<std::mutex> lock(events_mutex_);
  for (const auto& reg : binary_event_registrations_) {
    if ((reg.bucket == "*" || reg.bucket == event.bucket) &&
        (reg.key == "*" || reg.key == event.key)) {
      reg.callback(event);
    }
  }
}

void CachearooConnection::HandleResponse(const nlohmann::json& response) {
  std::string id = response.value("id", "");
  if (id.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(requests_mutex_);
  auto it = pending_requests_.find(id);
  if (it != pending_requests_.end()) {
    if (response.contains("error")) {
      std::string error = response["error"].dump();
      it->second->promise.set_exception(std::make_exception_ptr(std::runtime_error(error)));
    } else {
      std::string result;
      if (response.contains("value")) {
        result = response["value"].dump();
      } else if (response.contains("location")) {
        result = response["location"].get<std::string>();
      }
      it->second->promise.set_value(result);
    }
    pending_requests_.erase(it);
  }
}

void CachearooConnection::RegisterEvents() {
  if (!connected_.load())
    return;

  // Note: Caller must hold events_mutex_

  nlohmann::json event_array = nlohmann::json::array();
  std::set<std::string> unique_events;

  // Add regular events
  for (const auto& reg : event_registrations_) {
    std::string event_key = reg.bucket + "|" + reg.key;
    if (unique_events.find(event_key) == unique_events.end()) {
      nlohmann::json event_obj;
      event_obj["bucket"] = reg.bucket;
      event_obj["key"] = reg.key;
      event_array.push_back(event_obj);
      unique_events.insert(event_key);
    }
  }

  // Add binary events
  for (const auto& reg : binary_event_registrations_) {
    std::string event_key = reg.bucket + "|" + reg.key;
    if (unique_events.find(event_key) == unique_events.end()) {
      nlohmann::json event_obj;
      event_obj["bucket"] = reg.bucket;
      event_obj["key"] = reg.key;
      event_array.push_back(event_obj);
      unique_events.insert(event_key);
    }
  }

  nlohmann::json register_msg;
  register_msg["msg"] = "register-events";
  register_msg["events"] = event_array;
  register_msg["sendValues"] = send_values_;
  if (!settings_.api_key.empty()) {
    register_msg["apiKey"] = settings_.api_key;
  }

  if (ws_client_) {
    ws_client_->send(connection_hdl_, register_msg.dump(), websocketpp::frame::opcode::text);
  }
}

void CachearooConnection::CheckRequestTimeouts() {
  while (!skip_reconnect_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(requests_mutex_);

    for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
      auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->timestamp)
              .count();

      if (elapsed > kRequestTimeout) {
        it->second->promise.set_exception(
            std::make_exception_ptr(TimeoutError("Request timeout", false)));
        it = pending_requests_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void CachearooConnection::AddListener(const std::string& bucket, const std::string& key,
                                      bool send_values, EventCallback callback) {
  if (send_values)
    send_values_ = true;

  std::lock_guard<std::mutex> lock(events_mutex_);
  event_registrations_.push_back({bucket, key, std::move(callback)});
  RegisterEvents();
}

void CachearooConnection::AddBinaryListener(const std::string& bucket, const std::string& key,
                                            BinaryEventCallback callback) {
  std::lock_guard<std::mutex> lock(events_mutex_);
  binary_event_registrations_.push_back({bucket, key, std::move(callback)});
  RegisterEvents();
}

void CachearooConnection::RemoveListener(EventCallback /* callback */) {
  // Note: This is a simplified implementation. In a real implementation,
  // you might want to store callbacks with unique identifiers.
  std::lock_guard<std::mutex> lock(events_mutex_);
  // For now, we'll leave this as a placeholder since function comparison is complex
  RegisterEvents();
}

void CachearooConnection::RemoveBinaryListener(BinaryEventCallback /* callback */) {
  // Note: Similar to RemoveListener, this is simplified
  std::lock_guard<std::mutex> lock(events_mutex_);
  RegisterEvents();
}

void CachearooConnection::RemoveAllListeners() {
  std::lock_guard<std::mutex> lock(events_mutex_);
  event_registrations_.clear();
  binary_event_registrations_.clear();
  RegisterEvents();
}

std::string CachearooConnection::Read(const std::string& bucket, const std::string& key) {
  nlohmann::json request;
  request["op"] = "read";
  request["bucket"] = bucket;
  request["key"] = key;

  std::promise<std::string> promise;
  auto future = promise.get_future();
  AddRequest(request, promise);
  return future.get();
}

std::vector<ListReplyItem> CachearooConnection::List(const std::string& bucket, bool keys_only,
                                                     const std::string& filter) {
  nlohmann::json request;
  request["op"] = "filter";
  request["bucket"] = bucket;
  request["keysOnly"] = keys_only;
  if (!filter.empty()) {
    request["filter"] = filter;
  }

  std::promise<std::string> promise;
  auto future = promise.get_future();
  AddRequest(request, promise);
  std::string result = future.get();

  auto json_result = nlohmann::json::parse(result);
  std::vector<ListReplyItem> items;

  for (const auto& item : json_result) {
    ListReplyItem list_item;
    list_item.key = item.value("key", "");
    list_item.timestamp = item.value("timestamp", "");
    list_item.size = item.value("size", 0);
    if ((item.contains("expire")) && (!item["expire"].is_null())) {
      list_item.expire = item["expire"].get<std::string>();
    }
    if ((item.contains("content")) && (!item["content"].is_null())) {
      list_item.content = item["content"].dump();
    }
    items.push_back(list_item);
  }

  return items;
}

std::string CachearooConnection::Write(const std::string& bucket, const std::string& key,
                                       const std::string& value, bool fail_if_exists,
                                       const std::string& expire) {
  nlohmann::json request;
  request["op"] = "write";
  request["bucket"] = bucket;
  request["key"] = key;
  request["value"] = nlohmann::json::parse(value);
  if (fail_if_exists) {
    request["failIfExists"] = true;
  }
  if (!expire.empty()) {
    request["expire"] = expire;
  }

  std::promise<std::string> promise;
  auto future = promise.get_future();
  AddRequest(request, promise);
  return future.get();
}

std::string CachearooConnection::Patch(const std::string& bucket, const std::string& key,
                                       const std::string& patch, bool remove_data_from_reply) {
  nlohmann::json request;
  request["op"] = "patch";
  request["bucket"] = bucket;
  request["key"] = key;
  request["patch"] = nlohmann::json::parse(patch);
  if (remove_data_from_reply) {
    request["removeDataFromReply"] = true;
  }

  std::promise<std::string> promise;
  auto future = promise.get_future();
  AddRequest(request, promise);
  return future.get();
}

void CachearooConnection::Delete(const std::string& bucket, const std::string& key) {
  nlohmann::json request;
  request["op"] = "remove";
  request["bucket"] = bucket;
  request["key"] = key;

  std::promise<std::string> promise;
  auto future = promise.get_future();
  AddRequest(request, promise);
  future.get();  // Wait for completion
}

void CachearooConnection::SignalEvent(const std::string& bucket, const std::string& key,
                                      const std::string& value) {
  if (!connected_.load()) {
    throw std::runtime_error("Cannot signal event when disconnected");
  }

  nlohmann::json event_obj;
  event_obj["bucket"] = bucket;
  event_obj["key"] = key;
  event_obj["value"] = nlohmann::json::parse(value);

  nlohmann::json message;
  message["msg"] = "event-broadcast";
  if (!settings_.api_key.empty()) {
    message["apiKey"] = settings_.api_key;
  }
  message["event"] = event_obj;

  if (ws_client_) {
    ws_client_->send(connection_hdl_, message.dump(), websocketpp::frame::opcode::text);
  }
}

void CachearooConnection::SignalBinaryEvent(int type, const std::string& bucket,
                                            const std::string& key,
                                            const std::vector<uint8_t>& value) {
  if (!connected_.load()) {
    throw std::runtime_error("Cannot signal binary event when disconnected");
  }

  BinaryHeader header;
  header.type = static_cast<uint8_t>(type);
  header.bucket = bucket;
  header.key = key;
  header.api_key = settings_.api_key;

  auto header_data = HeaderToBuffer(header);
  std::vector<uint8_t> full_data;
  full_data.insert(full_data.end(), header_data.begin(), header_data.end());
  full_data.insert(full_data.end(), value.begin(), value.end());

  std::string binary_str(full_data.begin(), full_data.end());

  if (ws_client_) {
    ws_client_->send(connection_hdl_, binary_str, websocketpp::frame::opcode::binary);
  }
}

void CachearooConnection::AddRequest(const nlohmann::json& request,
                                     std::promise<std::string>& promise) {
  std::string id = std::to_string(++request_id_counter_);

  nlohmann::json full_request = request;
  full_request["id"] = id;

  nlohmann::json message;
  message["msg"] = "datastore-operation";
  if (!settings_.api_key.empty()) {
    message["apiKey"] = settings_.api_key;
  }
  message["request"] = full_request;

  {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto pending_request = std::make_unique<PendingRequest>();
    pending_request->id = id;
    pending_request->timestamp = std::chrono::steady_clock::now();
    pending_request->promise = std::move(promise);
    pending_requests_[id] = std::move(pending_request);
  }

  {
    std::lock_guard<std::mutex> lock(pending_message_mutex_);
    if (connected_.load() && ws_client_) {
      try {
        auto con = ws_client_->get_con_from_hdl(connection_hdl_);
        std::error_code ec = con->send(message.dump(), websocketpp::frame::opcode::text);
        if (ec) {
          throw std::runtime_error(ec.message());
        }
      } catch (const std::exception& e) {
        if (on_error_)
          on_error_(std::string("WebSocket send failed: ") + e.what());
        std::lock_guard<std::mutex> lock2(requests_mutex_);
        auto it = pending_requests_.find(id);
        if (it != pending_requests_.end()) {
          it->second->promise.set_exception(std::make_exception_ptr(
              std::runtime_error(std::string("WebSocket send failed: ") + e.what())));
        }
      }
    } else {
      pending_message_queue_.push_back(message.dump());
    }
  }
}

std::vector<uint8_t> CachearooConnection::HeaderToBuffer(const BinaryHeader& header) {
  std::vector<uint8_t> buffer(128, 0);

  buffer[0] = header.type;
  buffer[1] = static_cast<uint8_t>(std::min(header.bucket.size(), size_t(36)));
  buffer[2] = static_cast<uint8_t>(std::min(header.key.size(), size_t(36)));
  buffer[3] = static_cast<uint8_t>(std::min(header.api_key.size(), size_t(36)));

  size_t offset = 4;
  std::memcpy(buffer.data() + offset, header.bucket.data(), buffer[1]);
  offset += buffer[1];
  std::memcpy(buffer.data() + offset, header.key.data(), buffer[2]);
  offset += buffer[2];
  std::memcpy(buffer.data() + offset, header.api_key.data(), buffer[3]);

  return buffer;
}

BinaryHeader CachearooConnection::BufferToHeader(const std::vector<uint8_t>& buffer) {
  if (buffer.size() < 128) {
    throw std::runtime_error("Invalid binary header size");
  }

  BinaryHeader header;
  header.type = buffer[0];
  uint8_t bucket_size = buffer[1];
  uint8_t key_size = buffer[2];
  uint8_t api_key_size = buffer[3];

  size_t offset = 4;
  header.bucket = std::string(reinterpret_cast<const char*>(buffer.data() + offset), bucket_size);
  offset += bucket_size;
  header.key = std::string(reinterpret_cast<const char*>(buffer.data() + offset), key_size);
  offset += key_size;
  header.api_key = std::string(reinterpret_cast<const char*>(buffer.data() + offset), api_key_size);

  return header;
}

}  // namespace cachearoo