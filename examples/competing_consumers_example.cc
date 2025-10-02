#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

#include "cachearoo.h"

using namespace cachearoo;

// Job processing worker
class ImageProcessor : public Worker {
 public:
  explicit ImageProcessor(const std::string& id) : Worker(id) {
    SetWorkHandler([this](const std::string& job, MessageResponseCallback callback, ProgressCallback progress) {
      ProcessImage(job, callback, progress);
    });
  }
  
 private:
  void ProcessImage(const std::string& job, MessageResponseCallback callback, ProgressCallback progress) {
    try {
      auto job_json = nlohmann::json::parse(job);
      std::string image_path = job_json["image_path"];
      std::string filter = job_json["filter"];
      
      std::cout << "Worker " << GetId() << " processing image: " << image_path << " with filter: " << filter << std::endl;
      
      // Simulate image processing steps
      progress("Loading image...");
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      
      progress("Applying filter...");
      std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      
      progress("Saving result...");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      
      // Return result
      nlohmann::json result;
      result["processed_image"] = image_path + "_" + filter + "_processed.jpg";
      result["processing_time"] = "3.5 seconds";
      
      callback("", result.dump());
      
    } catch (const std::exception& e) {
      callback(e.what(), "");
    }
  }
};

void RunConsumer() {
  CachearooSettings settings;
  settings.host = "localhost";
  settings.port = 4300;
  settings.client_id = "image-processor-consumer";
  
  CachearooClient client(settings);
  
  // Wait for connection
  while (!client.IsConnected()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << "Consumer connected!" << std::endl;
  
  CompetingConsumer consumer(&client, "image-processing", settings.client_id);
  
  // Create some workers
  std::vector<std::shared_ptr<ImageProcessor>> workers;
  for (int i = 1; i <= 3; ++i) {
    workers.push_back(std::make_shared<ImageProcessor>("worker-" + std::to_string(i)));
  }
  
  // Set up job query handler
  consumer.SetJobQueryHandler([&workers](const std::string& job, JobQueryResponseCallback response) {
    try {
      auto job_json = nlohmann::json::parse(job);
      std::string filter = job_json["filter"];
      
      // Check if we support this filter type
      if (filter != "blur" && filter != "sharpen" && filter != "grayscale") {
        response(kJobNotSupported, nullptr);
        return;
      }
      
      // Find an available worker
      for (auto& worker : workers) {
        if (worker->IsAvailable()) {
          worker->SetAvailable(false);
          response(0, worker);
          return;
        }
      }
      
      // No workers available
      response(kNoWorkerAvailable, nullptr);
      
    } catch (const std::exception&) {
      response(-1, nullptr);
    }
  });
  
  // Set up job notification handler
  consumer.OnJob = [](const nlohmann::json& job_info) {
    if (job_info.contains("status")) {
      std::cout << "Job update: " << job_info["status"] << std::endl;
    } else if (job_info.contains("error")) {
      std::cout << "Job error: " << job_info["error"] << std::endl;
    } else if (job_info.contains("done") && job_info["done"]) {
      std::cout << "Job completed: " << job_info.dump() << std::endl;
    } else if (job_info.contains("progress")) {
      std::cout << "Job progress: " << job_info["progress"] << std::endl;
    }
  };
  
  std::cout << "Image processing consumer is running with " << workers.size() << " workers." << std::endl;
  std::cout << "Current job count: " << consumer.GetJobCount() << std::endl;
  std::cout << "Press Enter to stop..." << std::endl;
  std::cin.get();
  
  // Give time for any pending operations and close properly
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  client.Close();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void RunProducer() {
  CachearooSettings settings;
  settings.host = "localhost";
  settings.port = 4300;
  settings.client_id = "image-processor-producer";
  
  CachearooClient client(settings);
  
  // Wait for connection
  while (!client.IsConnected()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << "Producer connected!" << std::endl;
  
  Producer producer(&client, "image-processing");
  
  // Submit some image processing jobs
  std::vector<std::string> images = {
    "photo1.jpg",
    "photo2.jpg", 
    "photo3.jpg",
    "photo4.jpg"
  };
  
  std::vector<std::string> filters = {
    "blur",
    "sharpen", 
    "grayscale",
    "sepia"  // This one should fail as not supported
  };
  
  try {
    for (size_t i = 0; i < images.size(); ++i) {
      nlohmann::json job;
      job["image_path"] = images[i];
      job["filter"] = filters[i];
      job["id"] = "job-" + std::to_string(i + 1);
      
      std::cout << "Submitting job: " << job["id"] << " (" << images[i] << " with " << filters[i] << ")" << std::endl;
      
      // Submit job asynchronously with progress tracking
      auto future = producer.AddJobAsync(job.dump(), [](const std::string& progress) {
        std::cout << "Progress update: " << progress << std::endl;
      });
      
      // You could wait for completion or continue submitting
      std::thread([future = std::move(future), job_id = job["id"]]() mutable {
        try {
          std::string result = future.get();
          std::cout << "Job " << job_id << " completed: " << result << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Job " << job_id << " failed: " << e.what() << std::endl;
        }
      }).detach();
      
      // Small delay between submissions
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "All jobs submitted. Press Enter to exit..." << std::endl;
    std::cin.get();
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  
  // Give time for any pending operations and close properly
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  client.Close();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " [consumer|producer]" << std::endl;
    return 1;
  }
  
  std::string mode = argv[1];
  
  if (mode == "consumer") {
    RunConsumer();
  } else if (mode == "producer") {
    RunProducer();
  } else {
    std::cout << "Invalid mode. Use 'consumer' or 'producer'" << std::endl;
    return 1;
  }
  
  return 0;
}