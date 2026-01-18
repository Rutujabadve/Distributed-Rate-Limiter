# 🗺️ Code Tour: How to Read This Codebase

If you're new to this project (or just "vibe coded" it and forgot how it works), this guide is the recommended path to understanding the system.

## 1. The Contract (`ratelimiter.proto`)
**Start here.** This file defines the "API Surface" of your application.
*   **Location:** [ratelimiter.proto](file:///Users/rutujabadve/Desktop/projects/RateLimiter/ratelimiter.proto)
*   **What to look for:**
    *   The `RateLimiter` service definition.
    *   The `CheckLimit` function signature (Input: `user_id`, Output: `allowed`, `remaining`).
    *   This tells you *what* the system does, without worrying about *how* yet.

## 2. The Core Logic (`server.cpp`)
**This is the brain.** Once you know the inputs/outputs, see how they are processed.
*   **Location:** [server.cpp](file:///Users/rutujabadve/Desktop/projects/RateLimiter/server.cpp)
*   **Key Section: The Lua Script (Lines 17-47)**
    *   Read the `TOKEN_BUCKET_SCRIPT` string constant. This effectively runs "inside" Redis.
    *   Understand the math: `elapsed_time * refill_rate`.
*   **Key Section: `CheckLimit` Implementation (Lines 58-110)**
    *   See how it connects to Redis using `redis_.eval()`.
    *   Notice it catches exceptions (network/db errors) to prevent crashing.

## 3. The Client (`python_client.py` or `client.cpp`)
**See it in action.** Now that you know how the server works, see how a user consumes it.
*   **Location:** [python_client.py](file:///Users/rutujabadve/Desktop/projects/RateLimiter/python_client.py) (Easier to read)
    *   It's a simple loop sending 50 requests.
    *   Observe how it imports the generated `ratelimiter_pb2` code.
*   **Location:** [client.cpp](file:///Users/rutujabadve/Desktop/projects/RateLimiter/client.cpp) (For performance)
    *   Does the same thing but in C++. Shows how to use the C++ gRPC stub.

## 4. The Glue (`CMakeLists.txt`)
**How it builds.** Finally, understand the plumbing.
*   **Location:** [CMakeLists.txt](file:///Users/rutujabadve/Desktop/projects/RateLimiter/CMakeLists.txt)
*   **What to look for:**
    *   `find_package(gRPC)` and `find_package(Protobuf)`.
    *   The `protobuf_generate_grpc_cpp` command which auto-creates `.pb.cc` and `.pb.h` files from your proto.
    *   Linking libraries (`redis++`, `grpc++`, etc.).

---

## 💡 Key Concepts Review

1.  **gRPC**: The protocol used for communication (defined in `.proto`).
2.  **Redis**: The shared memory where token counts live (accessed in `server.cpp`).
3.  **Lua**: The atomic script ensuring no two requests race each other (embedded in `server.cpp`).
