#include "cachearoo_messaging.h"

#include <nlohmann/json.hpp>
#include <random>
#include <thread>
#include <condition_variable>

namespace cachearoo {

constexpr const char* kRequestReplyPrefix = "#reqrep/";
constexpr const char* kProgressPrefix = "#reqrep-progress/";
constexpr const char* kChannelPrefix = "messaging.compcon";
constexpr int kConsumerWaitRangeMs = 20;

// Requestor Implementation
Requestor::Requestor(CachearooClient* client, const std::string& channel, int timeout,
                     int progress_timeout)
    : client_(client), timeout_(timeout), progress_timeout_(progress_timeout) {
  channel_ = std::string(kRequestReplyPrefix) + channel;
  progress_channel_ = std::string(kProgressPrefix) + channel;

  // Set up event listeners
  reply_listener_ = [this](const Event& event) { OnReply(event); };
  progress_listener_ = [this](const Event& event) { OnProgress(event); };

  client_->GetConnection()->AddListener(channel_, "*", true, reply_listener_);
  client_->GetConnection()->AddListener(progress_channel_, "*", true, progress_listener_);

  // Start timeout checking thread
  timeout_thread_ = std::thread([this]() { CheckTimeouts(); });
}

Requestor::~Requestor() {
  Destroy();
}

void Requestor::Destroy() {
  shutdown_.store(true);

  if (timeout_thread_.joinable()) {
    timeout_thread_.join();
  }

  if (client_ && client_->GetConnection()) {
    client_->GetConnection()->RemoveListener(reply_listener_);
    client_->GetConnection()->RemoveListener(progress_listener_);
  }
}

std::future<std::string> Requestor::RequestAsync(const std::string& message,
                                                 ProgressCallback progress_callback, int timeout,
                                                 int progress_timeout) {
  if (timeout == 0)
    timeout = timeout_;
  if (progress_timeout == 0)
    progress_timeout = progress_timeout_;

  std::string id = GenerateUuid();

  // Signal the request event
  client_->GetConnection()->SignalEvent(channel_, id, message);

  auto pending_request = std::make_unique<PendingRequest>();
  auto now = std::chrono::steady_clock::now();
  pending_request->last_progress = now;
  pending_request->request_time = now;
  pending_request->progress_callback = progress_callback;

  auto future = pending_request->promise.get_future();

  {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    pending_requests_[id] = std::move(pending_request);
  }

  return future;
}

std::string Requestor::Request(const std::string& message, ProgressCallback progress_callback,
                               int timeout, int progress_timeout) {
  return RequestAsync(message, progress_callback, timeout, progress_timeout).get();
}

void Requestor::OnReply(const Event& event) {
  std::lock_guard<std::mutex> lock(requests_mutex_);
  auto it = pending_requests_.find(event.key);
  if (it != pending_requests_.end()) {
    if (event.value) {
      auto json_value = nlohmann::json::parse(*event.value);
      if (json_value.contains("err") && !json_value["err"].is_null()) {
        std::string error = json_value["err"].get<std::string>();
        it->second->promise.set_exception(std::make_exception_ptr(std::runtime_error(error)));
      } else {
        std::string data = json_value.value("data", "");
        it->second->promise.set_value(data);
      }
    }
    pending_requests_.erase(it);
  }
}

void Requestor::OnProgress(const Event& event) {
  std::lock_guard<std::mutex> lock(requests_mutex_);
  auto it = pending_requests_.find(event.key);
  if (it != pending_requests_.end()) {
    it->second->last_progress = std::chrono::steady_clock::now();
    if (it->second->progress_callback && event.value) {
      auto json_value = nlohmann::json::parse(*event.value);
      std::string data = json_value.value("data", "");
      it->second->progress_callback(data);
    }
  }
}

void Requestor::CheckTimeouts() {
  while (!shutdown_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(requests_mutex_);

    for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
      auto progress_elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->last_progress)
              .count();
      auto total_elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->request_time)
              .count();

      bool is_progress_timeout = progress_elapsed > progress_timeout_;
      bool is_timeout = total_elapsed > timeout_;

      if (is_progress_timeout || is_timeout) {
        it->second->promise.set_exception(std::make_exception_ptr(
            TimeoutError("Request timeout for id " + it->first, is_progress_timeout)));
        it = pending_requests_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

// Replier Implementation
Replier::Replier(CachearooClient* client, const std::string& channel) : client_(client) {
  channel_ = std::string(kRequestReplyPrefix) + channel;
  progress_channel_ = std::string(kProgressPrefix) + channel;

  request_listener_ = [this](const Event& event) { OnRequest(event); };
  client_->GetConnection()->AddListener(channel_, "*", true, request_listener_);
}

Replier::~Replier() {
  Destroy();
}

void Replier::Destroy() {
  if (client_ && client_->GetConnection()) {
    client_->GetConnection()->RemoveListener(request_listener_);
  }
}

void Replier::OnRequest(const Event& event) {
  if (!message_handler_ || !event.value)
    return;

  MessageResponseCallback response_callback = [this, key = event.key](const std::string& error,
                                                                      const std::string& data) {
    nlohmann::json response;
    response["err"] = error.empty() ? nullptr : nlohmann::json(error);
    response["data"] = data;
    client_->GetConnection()->SignalEvent(channel_, key, response.dump());
  };

  ProgressCallback progress_callback = [this, key = event.key](const std::string& data) {
    nlohmann::json progress;
    progress["data"] = data;
    client_->GetConnection()->SignalEvent(progress_channel_, key, progress.dump());
  };

  message_handler_(*event.value, response_callback, progress_callback);
}

// Producer Implementation
Producer::Producer(CachearooClient* client, const std::string& channel, int timeout,
                   int progress_timeout)
    : client_(client), timeout_(timeout), progress_timeout_(progress_timeout) {
  job_queue_ = std::string(kChannelPrefix) + "." + channel + ".jobs";
  status_queue_ = std::string(kChannelPrefix) + "." + channel + ".status";

  job_status_listener_ = [this](const Event& event) { OnJobStatus(event); };
  client_->GetConnection()->AddListener(status_queue_, "*", true, job_status_listener_);

  // Start timeout checking thread
  timeout_thread_ = std::thread([this]() { CheckTimeouts(); });
}

Producer::~Producer() {
  Destroy();
}

void Producer::Destroy() {
  shutdown_.store(true);

  if (timeout_thread_.joinable()) {
    timeout_thread_.join();
  }

  if (client_ && client_->GetConnection()) {
    client_->GetConnection()->RemoveListener(job_status_listener_);
  }
}

std::future<std::string> Producer::AddJobAsync(const std::string& job,
                                               ProgressCallback progress_callback, int timeout,
                                               int progress_timeout) {
  if (timeout == 0)
    timeout = timeout_;
  if (progress_timeout == 0)
    progress_timeout = progress_timeout_;

  std::string id = GenerateUuid();

  // Write job to queue
  client_->GetConnection()->Write(job_queue_, id, job);

  auto pending_job = std::make_unique<PendingJob>();
  auto now = std::chrono::steady_clock::now();
  pending_job->last_progress = now;  // Will be set to null initially, but simplified here
  pending_job->publish_time = now;
  pending_job->progress_callback = progress_callback;

  auto future = pending_job->promise.get_future();

  {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    pending_jobs_[id] = std::move(pending_job);
  }

  return future;
}

std::string Producer::AddJob(const std::string& job, ProgressCallback progress_callback,
                             int timeout, int progress_timeout) {
  return AddJobAsync(job, progress_callback, timeout, progress_timeout).get();
}

void Producer::OnJobStatus(const Event& event) {
  std::lock_guard<std::mutex> lock(jobs_mutex_);
  auto it = pending_jobs_.find(event.key);
  if (it != pending_jobs_.end()) {
    it->second->last_progress = std::chrono::steady_clock::now();

    if (event.value) {
      auto json_value = nlohmann::json::parse(*event.value);

      if (json_value.value("done", false)) {
        if (json_value.contains("err") && !json_value["err"].is_null()) {
          std::string error = json_value["err"].get<std::string>();
          it->second->promise.set_exception(std::make_exception_ptr(std::runtime_error(error)));
        } else {
          it->second->promise.set_value(json_value.dump());
        }

        // Clean up the status entry
        client_->GetConnection()->Delete(status_queue_, event.key);
        pending_jobs_.erase(it);
      } else {
        // Progress update
        if (it->second->progress_callback) {
          it->second->progress_callback(json_value.dump());
        }
      }
    }
  }
}

void Producer::CheckTimeouts() {
  while (!shutdown_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    for (auto it = pending_jobs_.begin(); it != pending_jobs_.end();) {
      auto progress_elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->last_progress)
              .count();
      auto total_elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->publish_time)
              .count();

      bool is_progress_timeout = progress_elapsed > progress_timeout_;
      bool is_timeout = total_elapsed > timeout_;

      if (is_progress_timeout || is_timeout) {
        it->second->promise.set_exception(std::make_exception_ptr(
            TimeoutError("Job timeout for id " + it->first, is_progress_timeout)));
        it = pending_jobs_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

// CompetingConsumer Implementation
CompetingConsumer::CompetingConsumer(CachearooClient* client, const std::string& channel,
                                     const std::string& client_id)
    : client_(client), client_id_(client_id) {
  job_queue_ = std::string(kChannelPrefix) + "." + channel + ".jobs";
  status_queue_ = std::string(kChannelPrefix) + "." + channel + ".status";

  job_queue_listener_ = [this](const Event& event) { OnJobReceived(event); };
  client_->GetConnection()->AddListener(job_queue_, "*", true, job_queue_listener_);
}

CompetingConsumer::~CompetingConsumer() {
  Destroy();
}

void CompetingConsumer::Destroy() {
  if (client_ && client_->GetConnection()) {
    client_->GetConnection()->RemoveListener(job_queue_listener_);
  }
}

void CompetingConsumer::OnJobReceived(const Event& event) {
  if (event.is_deleted || !event.value)
    return;

  try {
    auto job = nlohmann::json::parse(*event.value);

    // Notify about job received
    if (OnJob) {
      nlohmann::json job_notification;
      job_notification["id"] = job.value("id", "");
      job_notification["status"] = "Job received";
      OnJob(job_notification);
    }

    // Handle job in separate thread to avoid blocking
    std::thread([this, key = event.key, job]() { JobHandler(key, job); }).detach();

  } catch (const std::exception&) {
    if (OnJob) {
      nlohmann::json job_notification;
      job_notification["id"] = "unknown";
      job_notification["status"] = "No worker";
      OnJob(job_notification);
    }
  }
}

void CompetingConsumer::JobHandler(const std::string& key, const nlohmann::json& job) {
  if (OnJob) {
    nlohmann::json job_notification;
    job_notification["id"] = job.value("id", "");
    job_notification["status"] = "Job received";
    OnJob(job_notification);
  }

  // Wait a random amount to even out workload
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, kConsumerWaitRangeMs);
  std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

  auto worker = WaitForWorker(job);
  if (!worker)
    return;

  try {
    nlohmann::json status_obj;
    status_obj["claim"]["id"] = client_id_;
    status_obj["done"] = false;
    status_obj["err"] = nullptr;
    status_obj["data"] = nullptr;
    status_obj["progress"] = nullptr;

    // Try to claim the job
    client_->GetConnection()->Write(status_queue_, key, status_obj.dump(), true);
    client_->GetConnection()->Delete(job_queue_, key);

    job_count_++;

    // Execute the work
    auto work_handler = worker->GetWorkHandler();
    if (work_handler) {
      MessageResponseCallback callback = [this, key, status_obj](const std::string& error,
                                                                 const std::string& data) mutable {
        job_count_--;

        status_obj["err"] = error.empty() ? nullptr : nlohmann::json(error);
        status_obj["data"] = data;
        status_obj["done"] = true;

        client_->GetConnection()->Write(status_queue_, key, status_obj.dump());

        if (OnJob) {
          nlohmann::json job_notification;
          job_notification["id"] = "job_id";  // Should extract from job
          job_notification["done"] = true;
          job_notification["data"] = data;
          OnJob(job_notification);
        }
      };

      ProgressCallback progress_callback = [this, key,
                                            status_obj](const std::string& progress) mutable {
        status_obj["progress"] = progress;
        client_->GetConnection()->Write(status_queue_, key, status_obj.dump());

        if (OnJob) {
          nlohmann::json job_notification;
          job_notification["id"] = "job_id";  // Should extract from job
          job_notification["done"] = false;
          job_notification["progress"] = progress;
          OnJob(job_notification);
        }
      };

      work_handler(job.dump(), callback, progress_callback);
    }

  } catch (const AlreadyExistsError&) {
    // Job already taken by another consumer, release worker
    worker->Release();
  } catch (const std::exception&) {
    worker->Release();
    throw;
  }
}

std::shared_ptr<Worker> CompetingConsumer::WaitForWorker(const nlohmann::json& job) {
  if (!job_query_handler_)
    return nullptr;

  std::shared_ptr<Worker> result_worker;
  std::mutex wait_mutex;
  std::condition_variable wait_cv;
  bool worker_found = false;

  // Poll for available worker
  while (!worker_found) {
    std::promise<void> query_promise;
    auto query_future = query_promise.get_future();

    job_query_handler_(job.dump(), [&](int error_code, std::shared_ptr<Worker> worker) {
      std::lock_guard<std::mutex> lock(wait_mutex);

      if (error_code == 0 && worker) {
        result_worker = worker;
        worker_found = true;

        if (OnJob) {
          nlohmann::json job_notification;
          job_notification["id"] = job.value("id", "");
          job_notification["status"] = "Worker assigned";
          OnJob(job_notification);
        }
      } else if (error_code == kJobNotSupported) {
        worker_found = true;  // Stop trying

        if (OnJob) {
          nlohmann::json job_notification;
          job_notification["id"] = job.value("id", "");
          job_notification["error"] = "Not supported";
          OnJob(job_notification);
        }
      } else if (error_code == kNoWorkerAvailable) {
        // Continue polling
      } else {
        worker_found = true;  // Stop trying

        if (OnJob) {
          nlohmann::json job_notification;
          job_notification["id"] = job.value("id", "");
          job_notification["errno"] = error_code;
          OnJob(job_notification);
        }
      }

      query_promise.set_value();
      wait_cv.notify_one();
    });

    // Wait for the query to complete
    query_future.wait();

    if (!worker_found && !result_worker) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  return result_worker;
}

}  // namespace cachearoo