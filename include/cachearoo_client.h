#ifndef CACHEAROO_CLIENT_H_
#define CACHEAROO_CLIENT_H_

#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "cachearoo_types.h"
#include "cachearoo_connection.h"

namespace cachearoo {

class CachearooClient {
 public:
  explicit CachearooClient(const CachearooSettings& settings = CachearooSettings{});
  ~CachearooClient();

  // Data operations
  std::string Read(const std::string& key, const RequestOptions& options = {});
  std::vector<ListReplyItem> List(const RequestOptions& options = {});
  std::string Write(const std::string& key, const std::string& value,
                    const RequestOptions& options = {});
  std::string Patch(const std::string& key, const std::string& patch,
                    const RequestOptions& options = {});
  void Remove(const std::string& key, const RequestOptions& options = {});

  // Repair operation
  void Repair(const std::string& bucket);

  // Connection management
  void Close();
  bool IsConnected() const;

  // Access to underlying connection for messaging
  CachearooConnection* GetConnection() { return connection_.get(); }

 private:
  struct RequestQueueItem {
    std::promise<std::string> promise;
    RequestOptionsInternal options;
  };

  // Helper methods
  RequestOptionsInternal InternalizeRequestOptions(const std::string& key,
                                                   const RequestOptions& options);
  RequestOptions CheckOptions(const RequestOptions& options);
  std::string GetUrl(const std::string& key, const std::string& bucket, bool keys_only,
                     const std::string& filter);
  std::future<std::string> RequestHttp(const RequestOptionsInternal& options);
  std::future<std::string> Request(const RequestOptionsInternal& options);
  void ProcessRequestQueue();

  // Settings and connection
  CachearooSettings settings_;
  std::unique_ptr<CachearooConnection> connection_;

  // Request queue management
  std::queue<std::unique_ptr<RequestQueueItem>> request_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::thread queue_thread_;
  std::atomic<int> pending_requests_{0};
  std::atomic<bool> shutdown_{false};
};

}  // namespace cachearoo

#endif  // CACHEAROO_CLIENT_H_