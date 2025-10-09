#ifndef CACHEAROO_CLIENT_H_
#define CACHEAROO_CLIENT_H_

#include <memory>

#include "cachearoo_connection.h"
#include "cachearoo_types.h"

namespace cachearoo {

class CachearooClient {
 public:
  explicit CachearooClient(const CachearooSettings& settings = CachearooSettings{});
  ~CachearooClient();

  // Data operations
  std::string read(const std::string& key, const RequestOptions& options = {});
  std::vector<ListReplyItem> list(const RequestOptions& options = {});
  std::string write(const std::string& key, const std::string& value,
                    const RequestOptions& options = {});
  std::string patch(const std::string& key, const std::string& patch,
                    const RequestOptions& options = {});
  void remove(const std::string& key, const RequestOptions& options = {});

  // Repair operation
  void repair(const std::string& bucket);

  // Connection management
  void close();
  bool is_connected() const;

  // Access to underlying connection for messaging
  CachearooConnection* get_connection() { return connection_.get(); }

 private:
  // Helper methods
  RequestOptionsInternal internalize_request_options(const std::string& key,
                                                     const RequestOptions& options);
  RequestOptions check_options(const RequestOptions& options);
  std::string get_url(const std::string& key, const std::string& bucket, bool keys_only,
                      const std::string& filter);

  // Settings and connection
  CachearooSettings settings_;
  std::unique_ptr<CachearooConnection> connection_;
};

}  // namespace cachearoo

#endif  // CACHEAROO_CLIENT_H_