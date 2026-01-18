# --- Build Stage ---
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during install
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libhiredis-dev \
    libprotobuf-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the whole project
COPY . .

# 1. Build redis-plus-plus (since it's not a standard package)
WORKDIR /app/redis-plus-plus
RUN mkdir build && cd build && cmake .. && make && make install

# 2. Build our C++ Rate Limiter Server
WORKDIR /app
RUN mkdir build && cd build && cmake .. && make ratelimiter_server

# --- Run Stage ---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies (Python for proxy and libraries for C++ server)
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    libgrpc++ \
    libhiredis1.1 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built server from builder stage
COPY --from=builder /app/build/ratelimiter_server /app/ratelimiter_server
# Copy redis-plus-plus shared library
COPY --from=builder /usr/local/lib/libredis++.so* /usr/local/lib/
# Update library cache
RUN ldconfig

# Copy Python proxy and generated gRPC files
COPY proxy_server.py .
COPY ratelimiter_pb2.py .
COPY ratelimiter_pb2_grpc.py .

# Install Python dependencies
RUN pip3 install fastapi uvicorn grpcio pydantic

# Create a startup script to run BOTH the C++ server and Python proxy
RUN echo "#!/bin/bash\n./ratelimiter_server &\npython3 proxy_server.py" > start.sh
RUN chmod +x start.sh

# Expose the Python Proxy port (FastAPI) and gRPC port
EXPOSE 8000
EXPOSE 50051

# Start the system
CMD ["./start.sh"]
