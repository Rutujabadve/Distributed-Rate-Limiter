# --- Build Stage ---
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install only necessary build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libhiredis-dev \
    libprotobuf-dev \
    pkg-config \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy only the files needed for the builder to keep it small
# (The .dockerignore will handle excluding node_modules/build etc)
COPY . .

# Install grpcio-tools to generate python files
RUN pip3 install grpcio-tools

# Generate Python gRPC files
RUN python3 -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. ratelimiter.proto

# 1. Build redis-plus-plus
# We use a single thread to avoid memory spikes (OOM)
WORKDIR /app/redis-plus-plus
RUN mkdir -p build && cd build && \
    cmake -DREDIS_PLUS_PLUS_CXX_STANDARD=17 .. && \
    make -j1 && \
    make install

# 2. Build our C++ Rate Limiter Server
WORKDIR /app
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j1 ratelimiter_server

# --- Run Stage ---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime-only packages
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    libgrpc++1.33 \
    libhiredis1.1 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy only the final artifacts
COPY --from=builder /app/build/ratelimiter_server /app/ratelimiter_server
COPY --from=builder /usr/local/lib/libredis++.so* /usr/local/lib/
RUN ldconfig

# Copy bridge and gRPC files (COPY FROM BUILDER NOW)
COPY --from=builder /app/proxy_server.py .
COPY --from=builder /app/ratelimiter_pb2.py .
COPY --from=builder /app/ratelimiter_pb2_grpc.py .

# Install Python requirements
RUN pip3 install --no-cache-dir fastapi uvicorn grpcio pydantic

# Startup script
RUN echo "#!/bin/bash\n./ratelimiter_server &\nsleep 2\npython3 proxy_server.py" > start.sh
RUN chmod +x start.sh

EXPOSE 8000
EXPOSE 50051

CMD ["./start.sh"]
