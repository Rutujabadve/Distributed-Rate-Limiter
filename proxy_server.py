import grpc
import os
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import ratelimiter_pb2
import ratelimiter_pb2_grpc
import time

app = FastAPI()

# Enable CORS so our React app can talk to this server
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# This is our gRPC setup (Read from env for cloud)
GRPC_SERVER_ADDRESS = os.getenv('GRPC_SERVER_ADDRESS', 'localhost:50051')

class RateLimitRequest(BaseModel):
    user_id: str

@app.post("/check")
async def check_limit(request: RateLimitRequest):
    try:
        # Create a channel and a stub
        with grpc.insecure_channel(GRPC_SERVER_ADDRESS) as channel:
            stub = ratelimiter_pb2_grpc.RateLimiterStub(channel)
            
            # Make the actual call to your C++ server
            grpc_response = stub.CheckLimit(
                ratelimiter_pb2.RateLimitRequest(user_id=request.user_id)
            )
            
            return {
                "allowed": grpc_response.allowed,
                "remaining": grpc_response.remaining,
                "timestamp": time.time()
            }
    except grpc.RpcError as e:
        raise HTTPException(status_code=503, detail=f"gRPC Server Error: {e.code()}")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
