#include "cachearoo_client.h"

#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

#include <nlohmann/json.hpp>

namespace cachearoo {

CachearooClient::CachearooClient(const CachearooSettings& settings) : settings_(settings) {
  // Set default values if not provided
  if (settings_.host.empty()) {
    settings_.host = "127.0.0.1";
  }
  if (settings_.port == 0) {
    settings_.port = kDefaultPort;
  }

  // Ensure path starts and ends with '/'
  if (!settings_.path.empty()) {
    if (settings_.path.front() != '/') {
      settings_.path = "/" + settings_.path;
    }
    if (settings_.path.back() != '/') {
      settings_.path += "/";
    }
  } else {
    settings_.path = "/";
  }

  // Create connection
  connection_ = std::make_unique<CachearooConnection>(settings_);

  // Start request queue processing thread
  queue_thread_ = std::thread([this]() { ProcessRequestQueue(); });
}

CachearooClient::~CachearooClient() {
  Close();
}

void CachearooClient::Close() {
  shutdown_.store(true);
  queue_cv_.notify_all();

  if (queue_thread_.joinable()) {
    queue_thread_.join();
  }

  if (connection_) {
    connection_->Close();
  }
}

bool CachearooClient::IsConnected() const {
  return connection_ && connection_->IsConnected();
}

std::string CachearooClient::Read(const std::string& key, const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  opt.method = "GET";
  return Request(opt).get();
}

std::vector<ListReplyItem> CachearooClient::List(const RequestOptions& options) {
  auto opts_with_keys_only = options;
  if (!opts_with_keys_only.keys_only) {
    opts_with_keys_only.keys_only = true;
  }

  auto opt = InternalizeRequestOptions("", CheckOptions(opts_with_keys_only));
  opt.method = "GET";

  std::string result = Request(opt).get();
  auto json_result = nlohmann::json::parse(result);

  std::vector<ListReplyItem> items;
  for (const auto& item : json_result) {
    ListReplyItem list_item;
    list_item.key = item.value("key", "");
    list_item.timestamp = item.value("timestamp", "");
    list_item.size = item.value("size", 0);
    if (item.contains("expire")) {
      list_item.expire = item["expire"].get<std::string>();
    }
    if (item.contains("content")) {
      list_item.content = item["content"].dump();
    }
    items.push_back(list_item);
  }

  return items;
}

std::string CachearooClient::Write(const std::string& key, const std::string& value,
                                   const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  opt.method = opt.key.empty() ? "POST" : "PUT";
  opt.data = value;
  return Request(opt).get();
}

std::string CachearooClient::Patch(const std::string& key, const std::string& patch,
                                   const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  opt.method = "PATCH";
  opt.data = patch;
  return Request(opt).get();
}

void CachearooClient::Remove(const std::string& key, const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  opt.method = "DELETE";
  Request(opt).get();  // Wait for completion
}

RequestOptionsInternal CachearooClient::InternalizeRequestOptions(const std::string& key,
                                                                  const RequestOptions& options) {
  std::string bucket = options.bucket.value_or(settings_.bucket);
  bool async = options.async;

  RequestOptionsInternal opt;
  static_cast<RequestOptions&>(opt) = options;

  opt.url = GetUrl(key, bucket, options.keys_only, options.filter.value_or(""));
  opt.bucket = bucket;
  opt.key = key;
  opt.async = async;

  return opt;
}

RequestOptions CachearooClient::CheckOptions(const RequestOptions& options) {
  return options;  // Already a value type, no need to convert
}

std::string CachearooClient::GetUrl(const std::string& key, const std::string& bucket,
                                    bool keys_only, const std::string& filter) {
  std::string safe_key = key.empty() ? "" : key;

  // Add timestamp to prevent caching
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::string suffix = "d=" + std::to_string(time_t);
  suffix = (safe_key.find('?') != std::string::npos ? "&" : "?") + suffix;

  if (keys_only) {
    suffix += "&keysOnly=true";
  }

  if (!filter.empty()) {
    suffix += "&filter=" + filter;  // Should URL encode in production
  }

  std::string protocol = settings_.secure ? "https://" : "http://";
  std::string url = protocol + settings_.host + ":" + std::to_string(settings_.port) +
                    settings_.path + "_data/" + bucket + "/" + safe_key + suffix;

  return url;
}

std::future<std::string> CachearooClient::Request(const RequestOptionsInternal& options) {
  std::promise<std::string> promise;
  auto future = promise.get_future();

  auto queue_item = std::make_unique<RequestQueueItem>();
  queue_item->promise = std::move(promise);
  queue_item->options = options;

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    request_queue_.push(std::move(queue_item));
  }
  queue_cv_.notify_one();

  return future;
}

void CachearooClient::ProcessRequestQueue() {
  while (!shutdown_.load()) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return !request_queue_.empty() || shutdown_.load(); });

    if (shutdown_.load())
      break;

    // Check if we've hit the max concurrent limit
    if (pending_requests_.load() >= kMaxConcurrent) {
      continue;
    }

    if (request_queue_.empty())
      continue;

    auto item = std::move(request_queue_.front());
    request_queue_.pop();
    lock.unlock();

    pending_requests_++;

    std::thread([this, item = std::move(item)]() mutable {
      try {
        std::string result;
        // Always use WebSocket connection for all operations
        if (item->options.method == "GET") {
          if (item->options.key.empty()) {
            // List operation
            auto list_result =
                connection_->List(item->options.bucket.value_or(settings_.bucket),
                                  item->options.keys_only, item->options.filter.value_or(""));
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& list_item : list_result) {
              nlohmann::json item_json;
              item_json["key"] = list_item.key;
              item_json["timestamp"] = list_item.timestamp;
              item_json["size"] = list_item.size;
              if (list_item.expire) {
                item_json["expire"] = *list_item.expire;
              }
              if (list_item.content) {
                item_json["content"] = nlohmann::json::parse(*list_item.content);
              }
              json_array.push_back(item_json);
            }
            result = json_array.dump();
          } else {
            // Read operation
            result = connection_->Read(item->options.bucket.value_or(settings_.bucket),
                                       item->options.key);
          }
        } else if (item->options.method == "POST" || item->options.method == "PUT") {
          // Write operation
          result =
              connection_->Write(item->options.bucket.value_or(settings_.bucket), item->options.key,
                                 item->options.data.value_or("{}"), item->options.fail_if_exists,
                                 item->options.expire.value_or(""));
        } else if (item->options.method == "PATCH") {
          // Patch operation
          result = connection_->Patch(item->options.bucket.value_or(settings_.bucket),
                                      item->options.key, item->options.data.value_or("{}"),
                                      item->options.remove_data_from_reply);
        } else if (item->options.method == "DELETE") {
          // Delete operation
          connection_->Delete(item->options.bucket.value_or(settings_.bucket), item->options.key);
          result = "";  // Delete returns void
        }
        item->promise.set_value(result);
      } catch (const std::exception&) {
        item->promise.set_exception(std::current_exception());
      }
      pending_requests_--;
    }).detach();
  }
}

}  // namespace cachearoo