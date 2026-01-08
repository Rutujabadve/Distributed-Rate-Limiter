#!/bin/bash

echo "🧪 Running Distributed Rate Limiter Tests..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Check if Redis is running
if ! redis-cli ping &> /dev/null; then
    print_error "Redis is not running. Please start it with: brew services start redis"
    exit 1
fi
print_status "Redis is running"

# Check if binaries exist
if [ ! -f "build/ratelimiter_server" ]; then
    print_error "Server binary not found. Please build the project first."
    exit 1
fi
print_status "Server binary found"

if [ ! -f "ratelimiter_pb2.py" ]; then
    print_error "Python gRPC files not found. Please run setup.sh first."
    exit 1
fi
print_status "Python gRPC files found"

# Start server in background
echo "🚀 Starting server..."
cd build
./ratelimiter_server &
SERVER_PID=$!
cd ..
sleep 2

# Check if server is running
if ! ps -p $SERVER_PID > /dev/null; then
    print_error "Server failed to start"
    exit 1
fi
print_status "Server is running (PID: $SERVER_PID)"

# Run Python client test
echo "📊 Running Python client test..."
python3 python_client.py > test_output.log 2>&1
if [ $? -eq 0 ]; then
    print_status "Python client test completed"
else
    print_error "Python client test failed"
    cat test_output.log
    kill $SERVER_PID 2>/dev/null
    exit 1
fi

# Check test results
ALLOWED_COUNT=$(grep -c "ALLOWED" test_output.log)
DENIED_COUNT=$(grep -c "DENIED" test_output.log)

echo "📈 Test Results:"
echo "   - Allowed requests: $ALLOWED_COUNT"
echo "   - Denied requests: $DENIED_COUNT"

if [ "$ALLOWED_COUNT" -eq 50 ] && [ "$DENIED_COUNT" -eq 0 ]; then
    print_status "All tests passed! Rate limiter is working correctly."
else
    print_error "Test results unexpected. Expected 50 allowed, 0 denied."
    cat test_output.log
fi

# Test rate limit exhaustion
echo "🔥 Testing rate limit exhaustion..."
python3 -c "
import grpc
import ratelimiter_pb2 as rl
import ratelimiter_pb2_grpc as rl_grpc

with grpc.insecure_channel('localhost:50051') as channel:
    stub = rl_grpc.RateLimiterStub(channel)
    allowed = 0
    denied = 0
    for i in range(120):
        try:
            resp = stub.CheckLimit(rl.RateLimitRequest(user_id='exhaust_test'))
            if resp.allowed:
                allowed += 1
            else:
                denied += 1
        except:
            break
    
    print(f'Exhaustion test: {allowed} allowed, {denied} denied')
    if denied > 0:
        print('✅ Rate limiting working - bucket exhausted!')
    else:
        print('❌ Rate limiting not working - bucket not exhausted')
"

# Cleanup
kill $SERVER_PID 2>/dev/null
rm -f test_output.log

print_status "Test suite completed!"
