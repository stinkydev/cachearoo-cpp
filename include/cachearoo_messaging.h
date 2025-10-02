#ifndef CACHEAROO_MESSAGING_H_
#define CACHEAROO_MESSAGING_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "cachearoo_types.h"
#include "cachearoo_client.h"

namespace cachearoo {

// Forward declarations
class CachearooClient;

// Request-Reply Pattern Classes
class Requestor {
 public:
  Requestor(CachearooClient* client, const std::string& channel, int timeout = 5000,
            int progress_timeout = 5000);
  ~Requestor();

  std::future<std::string> RequestAsync(const std::string& message,
                                        ProgressCallback progress_callback = nullptr,
                                        int timeout = 0, int progress_timeout = 0);
  std::string Request(const std::string& message, ProgressCallback progress_callback = nullptr,
                      int timeout = 0, int progress_timeout = 0);
  void Destroy();

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
  EventCallback reply_listener_;
  EventCallback progress_listener_;
};

class Replier {
 public:
  Replier(CachearooClient* client, const std::string& channel);
  ~Replier();

  void SetMessageHandler(MessageCallback handler) { message_handler_ = std::move(handler); }
  void Destroy();

 private:
  void OnRequest(const Event& event);

  CachearooClient* client_;
  std::string channel_;
  std::string progress_channel_;
  MessageCallback message_handler_;
  EventCallback request_listener_;
};

// Competing Consumers Pattern Classes
class Worker {
 public:
  explicit Worker(const std::string& id) : id_(id), available_(true) {}

  const std::string& GetId() const { return id_; }
  bool IsAvailable() const { return available_.load(); }
  void SetAvailable(bool available) { available_.store(available); }
  void Release() { available_.store(true); }

  void SetWorkHandler(WorkCallback handler) { work_handler_ = std::move(handler); }
  WorkCallback GetWorkHandler() const { return work_handler_; }

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

  std::future<std::string> AddJobAsync(const std::string& job,
                                       ProgressCallback progress_callback = nullptr,
                                       int timeout = 0, int progress_timeout = 0);
  std::string AddJob(const std::string& job, ProgressCallback progress_callback = nullptr,
                     int timeout = 0, int progress_timeout = 0);
  void Destroy();

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
  EventCallback job_status_listener_;
};

class CompetingConsumer {
 public:
  CompetingConsumer(CachearooClient* client, const std::string& channel,
                    const std::string& client_id);
  ~CompetingConsumer();

  void SetJobQueryHandler(JobQueryCallback handler) { job_query_handler_ = std::move(handler); }
  int GetJobCount() const { return job_count_.load(); }
  void Destroy();

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
  EventCallback job_queue_listener_;
};

}  // namespace cachearoo

#endif  // CACHEAROO_MESSAGING_H_