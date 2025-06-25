#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include "pb_decode.h"
#include "pb_encode.h"
#include "rpc_envelope.pb.h"

// rpc_server.h  (or a small new header everyone can see)
#include <atomic>
extern std::atomic_bool g_server_ready;

extern "C" {
    int32_t receive_rpcmessage(uint32_t offset, uint32_t length);
    void    send_rpcresponse(uint32_t offset, uint32_t length, uint32_t request_id);
}

class RpcServer {
public:
    using HandlerFn = std::function<bool(uint32_t /*request_id*/, const RpcEnvelope&)>;

    RpcServer() = default;
    ~RpcServer() = default;

    void RegisterHandler(uint32_t payload_tag, HandlerFn handler);

    bool ProcessNextRequest();
    void sendResponse(const RpcResponse& resp, uint32_t request_id);

    // For typed handlers that operate on proto messages
    template<typename Req, typename Resp>
    void registerTypedHandler(uint32_t tag, std::function<void(const Req&, Resp*)> handler);
    void registerTypedHandler(uint32_t tag, std::function<void(const BatchSendMessage&, BatchResponse*)> handler);

    // Application-level native handlers (business logic only)
    void registerFunction(uint32_t tag, std::function<std::string(int32_t, std::string)> fn);  // SendMessage
    void registerFunction(uint32_t tag, std::function<int32_t(int32_t)> fn);                   // AddRandom
    void registerFunction(uint32_t tag, std::function<float(std::vector<float>)> fn);          // ProcessFloats
    void registerBatchFunction(uint32_t tag, std::function<std::string(int32_t, std::string)> fn); // BatchSendMessage

private:
    std::unordered_map<uint32_t, HandlerFn> handlers_;

    struct Buffer { uint8_t* data; uint32_t size; };
    Buffer alloc_buffer(uint32_t max_size);
};

// Payload extraction helpers (template specializations)
template<typename Req>
const Req& getPayload(uint32_t tag, const RpcEnvelope& env);

#endif // RPC_SERVER_H
