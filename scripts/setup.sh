#!/bin/bash

echo "🚀 Setting up Distributed Rate Limiter..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew not found. Please install Homebrew first: https://brew.sh/"
    exit 1
fi

echo "📦 Installing dependencies..."

# Install required packages
brew install redis protobuf grpc hiredis

# Check if redis-plus-plus needs installation
if ! brew list | grep -q redis-plus-plus; then
    echo "📦 Installing redis-plus-plus from source..."
    if [ ! -d "redis-plus-plus" ]; then
        git clone https://github.com/sewenew/redis-plus-plus.git
        cd redis-plus-plus
        mkdir build && cd build
        cmake -DCMAKE_INSTALL_PREFIX=../install .. && make && make install
        cd ../..
        cp -r redis-plus-plus/install redis-install
    fi
fi

# Install Python dependencies
pip3 install grpcio grpcio-tools

echo "🔧 Generating Python gRPC code..."
python3 -m grpc_tools.protoc --proto_path=. --python_out=. --grpc_python_out=. ratelimiter.proto

echo "🏗️ Building C++ binaries..."
mkdir -p build && cd build
cmake ..
make

echo "✅ Setup complete!"
echo "📋 Next steps:"
echo "1. Start Redis: brew services start redis"
echo "2. Start server: cd build && ./ratelimiter_server"
echo "3. Test client: python3 python_client.py"

