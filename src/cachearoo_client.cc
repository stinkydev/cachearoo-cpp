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

  // Create connection - it handles all async WebSocket communication internally
  connection_ = std::make_unique<CachearooConnection>(settings_);
}

CachearooClient::~CachearooClient() {
  Close();
}

void CachearooClient::Close() {
  if (connection_) {
    connection_->Close();
  }
}

bool CachearooClient::IsConnected() const {
  return connection_ && connection_->IsConnected();
}

std::string CachearooClient::Read(const std::string& key, const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  
  // Call connection directly - it handles async WebSocket communication internally
  return connection_->Read(opt.bucket.value_or(settings_.bucket), opt.key);
}

std::vector<ListReplyItem> CachearooClient::List(const RequestOptions& options) {
  auto opts_with_keys_only = options;
  if (!opts_with_keys_only.keys_only) {
    opts_with_keys_only.keys_only = true;
  }

  auto opt = InternalizeRequestOptions("", CheckOptions(opts_with_keys_only));
  
  // Call connection directly - it handles async WebSocket communication internally
  return connection_->List(opt.bucket.value_or(settings_.bucket), 
                          opt.keys_only.value_or(true), 
                          opt.filter.value_or(""));
}

std::string CachearooClient::Write(const std::string& key, const std::string& value,
                                   const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  
  // Call connection directly - it handles async WebSocket communication internally
  return connection_->Write(opt.bucket.value_or(settings_.bucket), 
                           opt.key, 
                           value, 
                           opt.fail_if_exists, 
                           opt.expire.value_or(""));
}

std::string CachearooClient::Patch(const std::string& key, const std::string& patch,
                                   const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  
  // Call connection directly - it handles async WebSocket communication internally
  return connection_->Patch(opt.bucket.value_or(settings_.bucket), 
                           opt.key, 
                           patch, 
                           opt.remove_data_from_reply);
}

void CachearooClient::Remove(const std::string& key, const RequestOptions& options) {
  auto opt = InternalizeRequestOptions(key, CheckOptions(options));
  
  // Call connection directly - it handles async WebSocket communication internally
  connection_->Delete(opt.bucket.value_or(settings_.bucket), opt.key);
}

RequestOptionsInternal CachearooClient::InternalizeRequestOptions(const std::string& key,
                                                                  const RequestOptions& options) {
  std::string bucket = options.bucket.value_or(settings_.bucket);
  bool async = options.async;

  RequestOptionsInternal opt;
  static_cast<RequestOptions&>(opt) = options;

  opt.url = GetUrl(key, bucket, options.keys_only.value_or(true), options.filter.value_or(""));
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

}  // namespace cachearoo