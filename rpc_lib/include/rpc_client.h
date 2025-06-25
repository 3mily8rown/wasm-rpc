#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "rpc_envelope.pb.h"

extern "C" {
    int32_t send_rpcmessage(uint32_t offset, uint32_t length);
    int32_t receive_rpcresponse(uint32_t offset, uint32_t length);

    // Multiple outstanding requests support
    int32_t send_rpcmessage_with_id(uint32_t offset, uint32_t length, uint32_t request_id); 
    int32_t receive_rpcresponse_with_id(uint32_t offset, uint32_t max_len, uint32_t request_id);
}

class RpcClient {
public:
    RpcClient() = default;

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

    // Internal helpers
    bool send(uint32_t request_id, uint32_t payload_tag, const void* message, bool is_async = false);
    
    template<typename T>
    bool decodeResponse(uint8_t* buf, size_t len, uint32_t expected_tag, uint32_t expected_request_id, T* out_msg);

    template<typename T>
    bool receive(uint32_t expected_tag, T* out_msg);

    template<typename T>
    bool pollResponse(uint32_t expected_tag, uint32_t req_id, T* out_msg);

    uint32_t next_request_id_ = 1;
};

#endif // RPC_CLIENT_H
