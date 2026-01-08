#!/usr/bin/env python3

import grpc
import time
import ratelimiter_pb2
import ratelimiter_pb2_grpc


def run_client():
    # Create a gRPC channel to the server
    with grpc.insecure_channel('localhost:50051') as channel:
        # Create a stub (client)
        stub = ratelimiter_pb2_grpc.RateLimiterStub(channel)

        print("Sending 50 requests to the C++ rate limiter server...")
        print("=" * 50)

        allowed_count = 0
        denied_count = 0

        for i in range(1, 51):  # Send 50 requests
            try:
                # Create a request
                request = ratelimiter_pb2.RateLimitRequest(user_id="python_client")

                # Make the gRPC call
                response = stub.CheckLimit(request)

                # Print the result
                status = "ALLOWED" if response.allowed else "DENIED"
                print(f"Request {i:2d}: {status} (remaining tokens: {response.remaining})")

                if response.allowed:
                    allowed_count += 1
                else:
                    denied_count += 1

            except grpc.RpcError as e:
                print(f"Request {i:2d}: RPC failed: {e}")

        print("=" * 50)
        print(f"Summary: {allowed_count} allowed, {denied_count} denied out of 50 requests")


if __name__ == '__main__':
    run_client()
