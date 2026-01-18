cmake_minimum_required(VERSION 3.10)
project(RateLimiter)

# Find required packages
find_package(Protobuf REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate C++ code from proto file
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ratelimiter.proto)

# Generate gRPC code
find_program(GRPC_CPP_PLUGIN NAMES grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    # Primary fallback for Mac/Homebrew if not in PATH
    set(GRPC_CPP_PLUGIN "/opt/homebrew/bin/grpc_cpp_plugin")
endif()

message(STATUS "Using gRPC C++ plugin: ${GRPC_CPP_PLUGIN}")

add_custom_command(
    OUTPUT ratelimiter.grpc.pb.cc ratelimiter.grpc.pb.h
    COMMAND protobuf::protoc
    ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
         --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         -I${CMAKE_CURRENT_SOURCE_DIR}
         ratelimiter.proto
    DEPENDS ratelimiter.proto
)

set(GRPC_SRCS ratelimiter.grpc.pb.cc)
set(GRPC_HDRS ratelimiter.grpc.pb.h)

# Create library with generated protobuf and gRPC code
add_library(ratelimiter_proto ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS})

# Include directories
target_include_directories(ratelimiter_proto
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

# Link protobuf libraries
target_link_libraries(ratelimiter_proto
    PUBLIC
        ${PROTOBUF_LIBRARIES}
)

# Find gRPC using pkg-config (avoids version conflicts)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# Find hiredis
pkg_check_modules(HIREDIS hiredis)
if(NOT HIREDIS_FOUND)
    # Fallback to manual search - PRIORITIZE /usr/local (where source builds go)
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h PATHS /usr/local/include /usr/include /opt/homebrew/include)
    find_library(HIREDIS_LIBRARY hiredis PATHS /usr/local/lib /usr/lib /opt/homebrew/lib)
else()
    set(HIREDIS_INCLUDE_DIR ${HIREDIS_INCLUDE_DIRS})
    set(HIREDIS_LIBRARY ${HIREDIS_LIBRARIES})
endif()

# Find redis-plus-plus
find_path(REDIS_PLUS_PLUS_INCLUDE_DIR sw/redis++/redis++.h PATHS 
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/redis-install/include
    /usr/include
)
find_library(REDIS_PLUS_PLUS_LIBRARY redis++ PATHS 
    /usr/local/lib
    ${CMAKE_SOURCE_DIR}/redis-install/lib
    /usr/lib
)

if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "hiredis not found.")
endif()

if(NOT REDIS_PLUS_PLUS_INCLUDE_DIR OR NOT REDIS_PLUS_PLUS_LIBRARY)
    message(FATAL_ERROR "redis-plus-plus not found.")
endif()

# Create server executable
add_executable(ratelimiter_server server.cpp)
target_include_directories(ratelimiter_server
    PRIVATE
        ${HIREDIS_INCLUDE_DIR}
        ${REDIS_PLUS_PLUS_INCLUDE_DIR}
        ${GRPC_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
)
target_link_libraries(ratelimiter_server
    PRIVATE
        ratelimiter_proto
        ${GRPC_LDFLAGS}
        ${PROTOBUF_LDFLAGS}
        ${HIREDIS_LIBRARY}
        ${REDIS_PLUS_PLUS_LIBRARY}
)

# Create test client executable
add_executable(ratelimiter_client client.cpp)
target_include_directories(ratelimiter_client
    PRIVATE
        ${GRPC_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
)
target_link_libraries(ratelimiter_client
    PRIVATE
        ratelimiter_proto
        ${GRPC_LDFLAGS}
        ${PROTOBUF_LDFLAGS}
)

# Create benchmark client executable
add_executable(benchmark_client benchmark_client.cpp)
target_include_directories(benchmark_client
    PRIVATE
        ${GRPC_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
)
target_link_libraries(benchmark_client
    PRIVATE
        ratelimiter_proto
        ${GRPC_LDFLAGS}
        ${PROTOBUF_LDFLAGS}
)
