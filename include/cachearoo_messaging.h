#ifndef CACHEAROO_MESSAGING_H_
#define CACHEAROO_MESSAGING_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "cachearoo_client.h"
#include "cachearoo_types.h"

namespace cachearoo {

// Forward declarations
class CachearooClient;

// Request-Reply Pattern Classes
class Requestor {
 public:
  Requestor(CachearooClient* client, const std::string& channel, int timeout = 5000,
            int progress_timeout = 5000);
  ~Requestor();

  std::future<std::string> request_async(const std::string& message,
                                         ProgressCallback progress_callback = nullptr,
                                         int timeout = 0, int progress_timeout = 0);
  std::string request(const std::string& message, ProgressCallback progress_callback = nullptr,
                      int timeout = 0, int progress_timeout = 0);
  void destroy();

 private:
  struct PendingRequest {
    std::chrono::steady_clock::time_point last_progress;
    std::chrono::steady_clock::time_point request_time;
    std::promise<std::string> promise;
    ProgressCallback progress_callback;
  };

  void OnReply(const Event& event);
  void OnProgress(const Event& event);
  void CheckTimeouts();

  CachearooClient* client_;
  std::string channel_;
  std::string progress_channel_;
  int timeout_;
  int progress_timeout_;

  std::map<std::string, std::unique_ptr<PendingRequest>> pending_requests_;
  std::mutex requests_mutex_;

  std::thread timeout_thread_;
  std::atomic<bool> shutdown_{false};
  int reply_listener_id_{-1};
  int progress_listener_id_{-1};
};

class Replier {
 public:
  Replier(CachearooClient* client, const std::string& channel);
  ~Replier();

  void set_message_handler(MessageCallback handler) { message_handler_ = std::move(handler); }
  void destroy();

 private:
  void OnRequest(const Event& event);

  CachearooClient* client_;
  std::string channel_;
  std::string progress_channel_;
  MessageCallback message_handler_;
  int request_listener_id_{-1};
};

// Competing Consumers Pattern Classes
class Worker {
 public:
  explicit Worker(const std::string& id) : id_(id), available_(true) {}

  const std::string& get_id() const { return id_; }
  bool is_available() const { return available_.load(); }
  void set_available(bool available) { available_.store(available); }
  void release() { available_.store(true); }

  void set_work_handler(WorkCallback handler) { work_handler_ = std::move(handler); }
  WorkCallback get_work_handler() const { return work_handler_; }

 private:
  std::string id_;
  std::atomic<bool> available_;
  WorkCallback work_handler_;
};

class Producer {
 public:
  Producer(CachearooClient* client, const std::string& channel, int timeout = 10000,
           int progress_timeout = 10000);
  ~Producer();

  std::future<std::string> add_job_async(const std::string& job,
                                         ProgressCallback progress_callback = nullptr,
                                         int timeout = 0, int progress_timeout = 0);
  std::string add_job(const std::string& job, ProgressCallback progress_callback = nullptr,
                      int timeout = 0, int progress_timeout = 0);
  void destroy();

 private:
  struct PendingJob {
    std::chrono::steady_clock::time_point last_progress;
    std::chrono::steady_clock::time_point publish_time;
    std::promise<std::string> promise;
    ProgressCallback progress_callback;
  };

  void OnJobStatus(const Event& event);
  void CheckTimeouts();

  CachearooClient* client_;
  std::string job_queue_;
  std::string status_queue_;
  int timeout_;
  int progress_timeout_;

  std::map<std::string, std::unique_ptr<PendingJob>> pending_jobs_;
  std::mutex jobs_mutex_;

  std::thread timeout_thread_;
  std::atomic<bool> shutdown_{false};
  int job_status_listener_id_{-1};
};

class CompetingConsumer {
 public:
  CompetingConsumer(CachearooClient* client, const std::string& channel,
                    const std::string& client_id);
  ~CompetingConsumer();

  void set_job_query_handler(JobQueryCallback handler) { job_query_handler_ = std::move(handler); }
  int get_job_count() const { return job_count_.load(); }
  void destroy();

  // Event callback for job notifications
  std::function<void(const nlohmann::json&)> OnJob;

 private:
  void OnJobReceived(const Event& event);
  void JobHandler(const std::string& key, const nlohmann::json& job);
  std::shared_ptr<Worker> WaitForWorker(const nlohmann::json& job);

  CachearooClient* client_;
  std::string job_queue_;
  std::string status_queue_;
  std::string client_id_;
  JobQueryCallback job_query_handler_;

  std::atomic<int> job_count_{0};
  int job_queue_listener_id_{-1};
};

}  // namespace cachearoo

#endif  // CACHEAROO_MESSAGING_H_