#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "ratelimiter.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <sw/redis++/connection_pool.h>
#include <sw/redis++/redis++.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Lua script for Token Bucket algorithm
const std::string TOKEN_BUCKET_SCRIPT = R"lua(
    local user_key = KEYS[1]
    local capacity = tonumber(ARGV[1])
    local refill_rate = tonumber(ARGV[2])

    -- Get current time in seconds (Redis TIME returns seconds and microseconds)
    local time_result = redis.call('TIME')
    local current_time = tonumber(time_result[1])

    -- Get current tokens and last refill time
    local current_tokens = tonumber(redis.call('HGET', user_key, 'tokens') or capacity)
    local last_refill = tonumber(redis.call('HGET', user_key, 'last_refill') or current_time)

    -- Calculate time elapsed and tokens to add
    local elapsed = current_time - last_refill
    local tokens_to_add = elapsed * refill_rate
    current_tokens = math.min(capacity, current_tokens + tokens_to_add)

    -- Check if request can be allowed
    local allowed = current_tokens >= 1

    if allowed then
        current_tokens = current_tokens - 1
    end

    -- Update the hash
    redis.call('HSET', user_key, 'tokens', current_tokens, 'last_refill', current_time)

    -- Return allowed status and remaining tokens
    return {allowed and 1 or 0, current_tokens}
)lua";

// Service implementation class
class RateLimiterServiceImpl final : public ratelimiter::RateLimiter::Service {
private:
  sw::redis::Redis redis_;

public:
  explicit RateLimiterServiceImpl(sw::redis::Redis redis)
      : redis_(std::move(redis)) {}

  Status CheckLimit(ServerContext *context,
                    const ratelimiter::RateLimitRequest *request,
                    ratelimiter::RateLimitResponse *response) override {
    // Start timing measurement
    auto start_time = std::chrono::high_resolution_clock::now();

    // Print request received message
    std::cout << "Request received for user: " << request->user_id()
              << std::endl;

    try {
      // Create Redis key for the user
      std::string user_key = "ratelimit:" + request->user_id();

      // Token bucket parameters (configurable per user or global)
      const int64_t capacity = 100;   // Maximum tokens
      const double refill_rate = 5.0; // Tokens per second

      // Execute the Lua script using redis.eval()
      // KEYS[1] = user_key
      // ARGV[1] = capacity, ARGV[2] = refill_rate
      auto result = redis_.eval<std::vector<long long>>(
          TOKEN_BUCKET_SCRIPT, {user_key},                        // KEYS
          {std::to_string(capacity), std::to_string(refill_rate)} // ARGV
      );

      // Parse the result: {allowed, remaining_tokens}
      bool allowed = result[0] == 1;
      int64_t remaining = result[1];

      // Set response
      response->set_allowed(allowed);
      response->set_remaining(remaining);

      std::cout << "Request " << (allowed ? "ALLOWED" : "DENIED")
                << " for user " << request->user_id()
                << ". Remaining tokens: " << remaining << std::endl;

      // End timing measurement
      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
          end_time - start_time);
      std::cout << "Time taken per request: " << duration.count()
                << " microseconds" << std::endl;

    } catch (const sw::redis::Error &e) {
      std::cerr << "Redis error in CheckLimit: " << e.what() << std::endl;
      return Status(grpc::INTERNAL, "Redis operation failed");
    } catch (const std::exception &e) {
      std::cerr << "Error in CheckLimit: " << e.what() << std::endl;
      return Status(grpc::INTERNAL, "Internal error");
    }

    return Status::OK;
  }
};

void RunServer(sw::redis::Redis redis) {
  const char *port = std::getenv("SERVER_PORT");
  std::string server_address =
      std::string("0.0.0.0:") + (port ? port : "50051");
  RateLimiterServiceImpl service(std::move(redis));

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char **argv) {
  try {
    // Redis connection options
    const char *redis_host = std::getenv("REDIS_HOST");
    const char *redis_port = std::getenv("REDIS_PORT");
    const char *redis_password = std::getenv("REDIS_PASSWORD");

    sw::redis::ConnectionOptions connection_opts;
    connection_opts.host = redis_host ? redis_host : "127.0.0.1";
    connection_opts.port = redis_port ? std::stoi(redis_port) : 6379;
    if (redis_password) {
      connection_opts.password = redis_password;
    }
    connection_opts.db = 0; // Database number

    // Redis connection pool options
    sw::redis::ConnectionPoolOptions pool_opts;
    pool_opts.size = 3; // Connection pool size

    // Create Redis connection with connection pool
    sw::redis::Redis redis(connection_opts, pool_opts);

    // Test connection (optional)
    redis.ping();

    std::cout << "Connected to Redis successfully" << std::endl;

    // Run the server with Redis connection
    RunServer(std::move(redis));

  } catch (const sw::redis::Error &e) {
    std::cerr << "Redis error: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
