#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "ratelimiter.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using ratelimiter::RateLimitRequest;
using ratelimiter::RateLimitResponse;
using ratelimiter::RateLimiter;

class RateLimiterClient {
public:
  RateLimiterClient(std::shared_ptr<Channel> channel)
      : stub_(RateLimiter::NewStub(channel)) {}

  // Test the rate limiter
  void TestRateLimit(const std::string& user_id, int num_requests, int delay_ms = 100) {
    std::cout << "\n=== Testing Rate Limiter for user: " << user_id << " ===\n";
    std::cout << "Token Bucket: 100 tokens capacity, 10 tokens/second refill rate\n\n";

    for (int i = 1; i <= num_requests; ++i) {
      RateLimitRequest request;
      request.set_user_id(user_id);

      RateLimitResponse response;
      ClientContext context;

      Status status = stub_->CheckLimit(&context, request, &response);

      if (status.ok()) {
        std::cout << "Request " << i << ": "
                  << (response.allowed() ? "ALLOWED" : "DENIED")
                  << " (remaining tokens: " << response.remaining() << ")" << std::endl;
      } else {
        std::cout << "Request " << i << ": RPC failed: " << status.error_message() << std::endl;
      }

      // Delay between requests
      if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      }
    }
  }

private:
  std::unique_ptr<RateLimiter::Stub> stub_;
};

int main(int argc, char** argv) {
  // Create client
  RateLimiterClient client(grpc::CreateChannel(
      "localhost:50051", grpc::InsecureChannelCredentials()));

  std::cout << "Testing Token Bucket Rate Limiter\n";
  std::cout << "=================================\n";

  // Test with different scenarios
  std::cout << "Test 1: Rapid requests (should consume tokens quickly)\n";
  client.TestRateLimit("user1", 15);

  std::cout << "\nTest 2: Wait for token refill, then test again\n";
  std::cout << "Waiting 2 seconds for token refill...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));
  client.TestRateLimit("user1", 5);

  std::cout << "\nTest 3: Different user (fresh bucket)\n";
  client.TestRateLimit("user2", 5);

  std::cout << "\nTest 4: Exhaust the bucket (no delays between requests)\n";
  client.TestRateLimit("user3", 120, 0);  // 120 requests, 0ms delay

  return 0;
}
