#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ratelimiter.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using ratelimiter::RateLimiter;
using ratelimiter::RateLimitRequest;
using ratelimiter::RateLimitResponse;

// Global atomic counters for statistics
std::atomic<long> successful_requests(0);
std::atomic<long> failed_requests(0);
std::atomic<long> allowed_requests(0);
std::atomic<long> denied_requests(0);

class Worker {
public:
  Worker(std::shared_ptr<Channel> channel, int requests_per_thread)
      : stub_(RateLimiter::NewStub(channel)),
        requests_to_send_(requests_per_thread) {}

  void Run() {
    RateLimitRequest request;
    request.set_user_id("benchmark_user");
    RateLimitResponse response;

    static std::atomic<bool> first_error_printed(false);

    for (int i = 0; i < requests_to_send_; ++i) {
      ClientContext context;
      Status status = stub_->CheckLimit(&context, request, &response);

      if (status.ok()) {
        successful_requests++;
        if (response.allowed()) {
          allowed_requests++;
        } else {
          denied_requests++;
        }
      } else {
        failed_requests++;
        if (!first_error_printed.exchange(true)) {
          std::cerr << "RPC Error Example (from thread "
                    << std::this_thread::get_id()
                    << "): " << status.error_message() << " ("
                    << status.error_code() << ")" << std::endl;
        }
      }
    }
  }

private:
  std::unique_ptr<RateLimiter::Stub> stub_;
  int requests_to_send_;
};

int main(int argc, char **argv) {
  int num_threads = 10;
  int requests_per_thread = 1000;

  // Parse arguments simple way
  if (argc > 1)
    num_threads = std::stoi(argv[1]);
  if (argc > 2)
    requests_per_thread = std::stoi(argv[2]);

  long total_requests = num_threads * requests_per_thread;

  std::cout << "Starting Benchmark..." << std::endl;
  std::cout << "Threads: " << num_threads << std::endl;
  std::cout << "Requests/Thread: " << requests_per_thread << std::endl;
  std::cout << "Total Requests: " << total_requests << std::endl;
  std::cout << "--------------------------------------------------"
            << std::endl;

  // Create channel once and share it (gRPC channels are thread-safe)
  // Using local unix socket or localhost
  auto channel = grpc::CreateChannel("localhost:50051",
                                     grpc::InsecureChannelCredentials());

  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<Worker>> workers;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Spawn threads
  for (int i = 0; i < num_threads; ++i) {
    workers.push_back(std::make_unique<Worker>(channel, requests_per_thread));
    threads.emplace_back(&Worker::Run, workers.back().get());
  }

  // Wait for threads
  for (auto &t : threads) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();

  // Calculate Stats
  std::chrono::duration<double> duration = end_time - start_time;
  double seconds = duration.count();
  double rps = total_requests / seconds;

  std::cout << "Time Elapsed: " << std::fixed << std::setprecision(4) << seconds
            << " seconds" << std::endl;
  std::cout << "Total Requests: " << successful_requests + failed_requests
            << std::endl;
  std::cout << "  - Successful (RPC OK): " << successful_requests << std::endl;
  std::cout << "    - Allowed: " << allowed_requests << std::endl;
  std::cout << "    - Denied:  " << denied_requests << std::endl;
  std::cout << "  - Failed (RPC Error):  " << failed_requests << std::endl;
  std::cout << "--------------------------------------------------"
            << std::endl;
  std::cout << "Throughput (RPS): " << std::fixed << std::setprecision(2) << rps
            << " req/s" << std::endl;
  std::cout << "--------------------------------------------------"
            << std::endl;

  return 0;
}
