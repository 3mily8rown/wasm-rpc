#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "rpc_envelope.pb.h"
#include <thread>
#include <atomic>

class RpcClient {
public:
    RpcClient();
    ~RpcClient();

    // Public RPC interface
    std::string sendMessage(int32_t id, const char* name);           // Returns ack.info
    int32_t addRandom(int32_t num);                                  // Returns sum
    float processFloats(const float* arr, size_t count);             // Returns sum
    float processFloats(const std::vector<float>& vec);              // Convenience wrapper

    // Asynchronous methods
    uint32_t sendMessageAsync(int32_t id, const char* name);
    bool pollSendMessageResponse(uint32_t request_id, std::string& result);

    // Batch methods
    uint32_t sendMessageBatch(const std::vector<std::string>& messages);
    bool waitForBatchResponse(uint32_t id, std::unordered_map<uint32_t, std::string>& results);

private:
    struct PendingRequest {
        uint32_t tag;
        uint32_t request_id;
    };
    std::unordered_map<uint32_t, PendingRequest> pending_;

    void runResponseListener();

    // Internal helpers
    bool send(uint32_t request_id, uint32_t payload_tag, const void* message, bool is_async = false);
    
    template<typename T>
    bool decodeResponse(uint8_t* buf, size_t len, uint32_t expected_tag, uint32_t expected_request_id, T* out_msg);

    template<typename T>
    bool receive(uint32_t expected_tag, T* out_msg);

    template<typename T>
    bool pollResponse(uint32_t expected_tag, uint32_t req_id, T* out_msg);

    // Optional: ID counter if you want to auto-generate request IDs
    uint32_t next_request_id_ = 1;

    void backgroundListener();
    std::thread listener_thread_;
    std::atomic<bool> running_;
};

#endif // RPC_CLIENT_H
