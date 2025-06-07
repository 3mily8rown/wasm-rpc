#include "rpc_client.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pb.h"
#include "pb_decode.h"  // Required for pb_istream_from_buffer
#include "rpc_envelope.pb.h"  // Includes ProcessFloats struct
#include "pb_encode.h"
#include <iostream>

#define PB_ARRAY_SIZE(s, field) (sizeof((s)->field) / sizeof((s)->field[0]))

// ----------------------------
// Public API methods
// I think typical RPC libraries would generate these methods
// based on the protobuf definitions.
// The methods here are manually implemented for clarity and simplicity.
// ----------------------------

std::string RpcClient::sendMessage(int32_t id, const char* name) {
    SendMessage msg = SendMessage_init_zero;
    msg.id = id;
    std::strncpy(msg.name, name, sizeof(msg.name) - 1);

    if (!send(next_request_id_++, RpcEnvelope_msg_tag, &msg)) return {};

    SendMessageResponse resp = SendMessageResponse_init_zero;
    if (!receive<SendMessageResponse>(RpcResponse_msg_tag, &resp)) return {};

    return std::string(resp.info);
}

int32_t RpcClient::addRandom(int32_t num) {
    AddRandom msg = AddRandom_init_zero;
    msg.num = num;

    if (!send(next_request_id_++, RpcEnvelope_rand_tag, &msg)) return -1;

    AddRandomResponse resp = AddRandomResponse_init_zero;
    if (!receive<AddRandomResponse>(RpcResponse_rand_tag, &resp)) return -1;

    return resp.sum;
}

float RpcClient::processFloats(const float* arr, size_t count) {
    ProcessFloats msg = ProcessFloats_init_zero;

    if (count > PB_ARRAY_SIZE(&msg, num)) {
        std::fprintf(stderr, "Too many floats (max=%zu)\n", PB_ARRAY_SIZE(&msg, num));
        return -1.0f;
    }

    msg.num_count = count;
    std::memcpy(msg.num, arr, count * sizeof(float));

    if (!send(next_request_id_++, RpcEnvelope_flt_tag, &msg)) return -1.0f;

    ProcessFloatsResponse resp = ProcessFloatsResponse_init_zero;
    if (!receive<ProcessFloatsResponse>(RpcResponse_flt_tag, &resp)) return -1.0f;

    return resp.sum;
}


float RpcClient::processFloats(const std::vector<float>& vec) {
    return processFloats(vec.data(), vec.size());
}

// Asynchronous methods
uint32_t RpcClient::sendMessageAsync(int32_t id, const char* name) {
    SendMessage msg = SendMessage_init_zero;
    msg.id = id;
    std::strncpy(msg.name, name, sizeof(msg.name) - 1);

    uint32_t req_id = next_request_id_++;
    if (!send(req_id, RpcEnvelope_msg_tag, &msg, true)) return 0;

    pending_[req_id] = {RpcResponse_msg_tag, req_id};
    return req_id;
}
bool RpcClient::pollSendMessageResponse(uint32_t request_id, std::string& result) {
    SendMessageResponse resp = SendMessageResponse_init_zero;
    if (!pollResponse<SendMessageResponse>(RpcResponse_msg_tag, request_id, &resp)) return false;
    result = std::string(resp.info);
    pending_.erase(request_id);
    return true;
}

// Batch methods
uint32_t RpcClient::sendMessageBatch(const std::vector<std::string>& messages) {
    BatchSendMessage batch = BatchSendMessage_init_zero;

    size_t count = std::min(messages.size(), static_cast<size_t>(PB_ARRAY_SIZE(&batch, messages)));
    for (size_t i = 0; i < count; ++i) {
        batch.messages[i].id = static_cast<int32_t>(i);  // optional
        std::strncpy(batch.messages[i].name, messages[i].c_str(), sizeof(batch.messages[i].name) - 1);
    }
    batch.messages_count = count;

    uint32_t req_id = next_request_id_++;
    if (!send(req_id, RpcEnvelope_batch_msg_tag, &batch, true)) return 0;

    pending_[req_id] = {RpcResponse_batch_msg_tag, req_id};
    return req_id;
}

bool RpcClient::waitForBatchResponse(uint32_t id, std::unordered_map<uint32_t, std::string>& results) {
    BatchResponse resp = BatchResponse_init_zero;
    if (!pollResponse<BatchResponse>(RpcResponse_batch_msg_tag, id, &resp)) {
        std::fprintf(stderr, "[waitForBatchResponse] Failed to receive batch response\n");
        return false;
    }

    for (size_t i = 0; i < resp.responses_count; ++i) {
        results[resp.responses[i].id] = std::string(resp.responses[i].info);
    }
    return true;
}

// ----------------------------
// Internal helpers
// ----------------------------

bool RpcClient::send(uint32_t request_id, uint32_t payload_tag, const void* message, bool is_async) {
    RpcEnvelope env = RpcEnvelope_init_zero;
    env.request_id = request_id;
    env.which_payload = payload_tag;

    switch (payload_tag) {
        case RpcEnvelope_msg_tag:
            env.payload.msg = *reinterpret_cast<const SendMessage*>(message);
            break;
        case RpcEnvelope_rand_tag:
            env.payload.rand = *reinterpret_cast<const AddRandom*>(message);
            break;
        case RpcEnvelope_flt_tag:
            env.payload.flt = *reinterpret_cast<const ProcessFloats*>(message);
            break;
        case RpcEnvelope_batch_msg_tag:
            env.payload.batch_msg = *reinterpret_cast<const BatchSendMessage*>(message);
            break;
        default:
            std::fprintf(stderr, "Unknown payload_tag: %u\n", payload_tag);
            return false;
    }

    // shift the buffer to leave space for the request ID
    uint8_t buf[1024];
    pb_ostream_t stream = pb_ostream_from_buffer(buf + 4, sizeof(buf) - 4);

    if (!pb_encode(&stream, RpcEnvelope_fields, &env)) {
        std::fprintf(stderr, "RpcEnvelope encode error: %s\n", PB_GET_ERROR(&stream));
        return false;
    }

    uint32_t len = stream.bytes_written;
    uint8_t* wasm_buf = static_cast<uint8_t*>(std::malloc(len));
    if (!wasm_buf) {
        std::fprintf(stderr, "malloc failed\n");
        return false;
    }

    std::memset(wasm_buf, 0, 4);
    std::memcpy(wasm_buf + 4, buf + 4, len);
    int32_t rc = is_async
        ? send_rpcmessage_with_id(reinterpret_cast<uint32_t>(wasm_buf), len + 4, request_id)
        : send_rpcmessage(reinterpret_cast<uint32_t>(wasm_buf), len + 4);
    std::free(wasm_buf);

    return rc >= 0;
}

template<typename T>
bool RpcClient::decodeResponse(uint8_t* buf, size_t len, uint32_t expected_tag, uint32_t expected_request_id, T* out_msg) {
    RpcResponse resp = RpcResponse_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(buf, len);

    if (!pb_decode(&stream, RpcResponse_fields, &resp)) {
        std::fprintf(stderr, "Response decode error: %s\n", PB_GET_ERROR(&stream));
        return false;
    }

    // if (expected_request_id != UINT32_MAX && resp.request_id != expected_request_id) {
    //     std::fprintf(stderr, "Unexpected request ID: %u, expected: %u\n", resp.request_id, expected_request_id);
    //     return false;
    // }

    if (resp.which_payload != expected_tag) {
        std::fprintf(stderr, "Unexpected payload tag: %u, expected: %u\n", resp.which_payload, expected_tag);
        return false;
    }

    if constexpr (std::is_same_v<T, SendMessageResponse>) {
        *out_msg = resp.payload.msg;
    } else if constexpr (std::is_same_v<T, AddRandomResponse>) {
        *out_msg = resp.payload.rand;
    } else if constexpr (std::is_same_v<T, ProcessFloatsResponse>) {
        *out_msg = resp.payload.flt;
    } else if constexpr (std::is_same_v<T, BatchResponse>) {
        *out_msg = resp.payload.batch_msg;
    } else {
        static_assert(sizeof(T) == 0, "Unsupported message type");
    }

    return true;
}


template<typename T>
bool RpcClient::receive(uint32_t expected_tag, T* out_msg) {
    uint8_t* buf = static_cast<uint8_t*>(std::malloc(512));
    if (!buf) return false;

    int32_t len = receive_rpcresponse(reinterpret_cast<uint32_t>(buf), 512);
    bool success = (len > 0) && decodeResponse<T>(buf, len, expected_tag, UINT32_MAX, out_msg);

    std::free(buf);
    return success;
}

template<typename T>
bool RpcClient::pollResponse(uint32_t expected_tag, uint32_t request_id, T* out_msg) {
    auto it = pending_.find(request_id);
    if (it == pending_.end() || it->second.tag != expected_tag) { 
        std::fprintf(stderr, "No pending request with ID %u and tag %u\n", request_id, expected_tag);
        return false;
    }

    uint8_t* buf = static_cast<uint8_t*>(std::malloc(512));
    if (!buf) {
        std::fprintf(stderr, "malloc failed\n");
        return false;
    }

    int32_t len = receive_rpcresponse_with_id(reinterpret_cast<uint32_t>(buf), 512, request_id);
    if (len == 0) {
        // std::fprintf(stderr, "response not ready yet for request_id %u\n", request_id);
        std::free(buf);
        return false;
    }

    bool success = (len > 0) && decodeResponse<T>(buf, len, expected_tag, request_id, out_msg);
    if (!success) {
        std::fprintf(stderr, "Failed to decode response for request_id %u\n", request_id);
    }

    std::free(buf);
    return success;
}

// Explicit template instantiations for the types used
template bool RpcClient::receive<SendMessageResponse>(uint32_t, SendMessageResponse*);
template bool RpcClient::receive<AddRandomResponse>(uint32_t, AddRandomResponse*);
template bool RpcClient::receive<ProcessFloatsResponse>(uint32_t, ProcessFloatsResponse*);
template bool RpcClient::receive<BatchResponse>(uint32_t, BatchResponse*);
