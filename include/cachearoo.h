#ifndef CACHEAROO_H_
#define CACHEAROO_H_

// Main include file for Cachearoo C++ library

#include "cachearoo_types.h"
#include "cachearoo_client.h" 
#include "cachearoo_connection.h"
#include "cachearoo_messaging.h"

namespace cachearoo {

// Export main classes
using Cachearoo = CachearooClient;

// Messaging namespace
namespace messaging {

namespace request_reply {
using Requestor = cachearoo::Requestor;
using Replier = cachearoo::Replier;
}  // namespace request_reply

namespace competing_consumers {
using CompetingConsumer = cachearoo::CompetingConsumer;
using Producer = cachearoo::Producer;
}  // namespace competing_consumers

namespace errors {
constexpr int kJobNotSupported = cachearoo::kJobNotSupported;
constexpr int kNoWorkerAvailable = cachearoo::kNoWorkerAvailable;
}  // namespace errors

}  // namespace messaging

}  // namespace cachearoo

#endif  // CACHEAROO_H_