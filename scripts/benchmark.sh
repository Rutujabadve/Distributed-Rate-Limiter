#!/bin/bash

echo "📊 Distributed Rate Limiter Benchmark"
echo "===================================="

# Colors
GREEN='\033[0;32m'
NC='\033[0m'

# Start server if not running
if ! pgrep -f "ratelimiter_server" > /dev/null; then
    echo "🚀 Starting server..."
    cd build
    ./ratelimiter_server &
    SERVER_PID=$!
    cd ..
    sleep 2
fi

echo "⏱️ Running benchmark..."

# Run benchmark test
python3 -c "
import grpc
import time
import ratelimiter_pb2 as rl
import ratelimiter_pb2_grpc as rl_grpc

def benchmark_requests(num_requests, user_id):
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = rl_grpc.RateLimiterStub(channel)

        start_time = time.time()
        allowed = 0
        denied = 0

        for i in range(num_requests):
            try:
                resp = stub.CheckLimit(rl.RateLimitRequest(user_id=user_id))
                if resp.allowed:
                    allowed += 1
                else:
                    denied += 1
            except Exception as e:
                print(f'Error: {e}')
                break

        end_time = time.time()
        total_time = end_time - start_time
        avg_time = (total_time / num_requests) * 1000  # ms

        return {
            'requests': num_requests,
            'allowed': allowed,
            'denied': denied,
            'total_time': total_time,
            'avg_time': avg_time,
            'throughput': num_requests / total_time
        }

# Single user test
print('Testing single user (1000 requests)...')
result = benchmark_requests(1000, 'bench_user')
print(f'  Requests: {result[\"requests\"]}')
print(f'  Allowed: {result[\"allowed\"]}, Denied: {result[\"denied\"]}')
print(f'  Total time: {result[\"total_time\"]:.2f}s')
print(f'  Avg latency: {result[\"avg_time\"]:.2f}ms')
print(f'  Throughput: {result[\"throughput\"]:.0f} req/s')
print()

# Multiple users test
print('Testing multiple users (100 requests each for 10 users)...')
start_time = time.time()
total_requests = 0
total_allowed = 0
total_denied = 0

for user_id in range(10):
    result = benchmark_requests(100, f'multi_user_{user_id}')
    total_requests += result['requests']
    total_allowed += result['allowed']
    total_denied += result['denied']

end_time = time.time()
total_time = end_time - start_time
avg_latency = (total_time / total_requests) * 1000
throughput = total_requests / total_time

print(f'  Total requests: {total_requests}')
print(f'  Allowed: {total_allowed}, Denied: {total_denied}')
print(f'  Total time: {total_time:.2f}s')
print(f'  Avg latency: {avg_latency:.2f}ms')
print(f'  Throughput: {throughput:.0f} req/s')
"

# Cleanup
if [ ! -z "$SERVER_PID" ]; then
    kill $SERVER_PID 2>/dev/null
fi

echo ""
echo -e "${GREEN}✅ Benchmark completed!${NC}"
