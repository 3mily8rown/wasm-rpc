// rpc_transport.cpp
#include "rpc_transport.h"
#include "rpc/socket_communication.h"
#include "ring_buffer_rpc/rpc_messaging.h"
#include "rpc/request_id_utils.h"
#include <cstring>
#include <iostream>

int32_t receive_rpcresponse_internal_nonblocking(uint8_t* dest, uint32_t max_len, uint32_t request_id);
int32_t receive_rpcresponse_internal(uint8_t* dest, uint32_t max_len, uint32_t request_id);

void add_request_id_to_message(uint8_t* message, uint32_t request_id) {
    std::memcpy(message, &request_id, sizeof(uint32_t));
}

// todo - this is a placeholder function
uint32_t get_client_id() {
    return 42; 
}

// Client-side transport functions

int send_rpcmessage(void* ptr, int len) {
    // std::cout << "[send_rpcmessage] Sending " << len << " bytes from " << ptr << std::endl;
    add_request_id_to_message(static_cast<uint8_t*>(ptr), get_client_id());
    begin_wait_for_response(get_client_id());
    send_over_socket(static_cast<uint8_t*>(ptr), len);
    return 0;
}

int send_rpcmessage_with_id(void* ptr, int len, uint32_t id) {
    uint32_t full_id = make_full_id(id, get_client_id());
    // std::cout << "[send_rpcmessage_with_id] Full ID: " << full_id << std::endl;
    add_request_id_to_message(static_cast<uint8_t*>(ptr),  full_id);
    begin_wait_for_response(full_id);
    send_over_socket(static_cast<uint8_t*>(ptr), len);
    return 0;
}

int receive_rpcresponse(void* ptr, int max_len) {
    // std::cout << "[receive_rpcresponse] Receiving up to " << max_len << " bytes into " << ptr << std::endl;
    return receive_rpcresponse_internal(static_cast<uint8_t*>(ptr), max_len, get_client_id());
}

int receive_rpcresponse_with_id(void* ptr, int max_len, uint32_t id) {
    // std::cout << "[receive_rpcresponse_with_id] Receiving up to " << max_len
    //           << " bytes into " << ptr << " for ID " << id << std::endl;
    uint32_t full_id = make_full_id(id, get_client_id());
    return receive_rpcresponse_internal_nonblocking(static_cast<uint8_t*>(ptr), max_len, full_id);
}

// Server-side transport functions

int32_t receive_rpcmessage(void* ptr, uint32_t length) {
    // std::cout << "[receive_rpcmessage] Receiving up to " << length << " bytes into " << ptr << std::endl;
    return dequeue_message_with_timeout(static_cast<uint8_t*>(ptr), length, 10000);
}

void send_rpcresponse(void* ptr, uint32_t length, uint32_t request_id) {
    // std::cout << "[send_rpcresponse] Sending " << length << " bytes from " << ptr
    //           << " for request ID " << request_id << std::endl;
    send_response_over_socket(static_cast<uint8_t*>(ptr), length, request_id);
}

// Internal helper functions

int32_t receive_rpcresponse_internal(uint8_t* dest, uint32_t max_len, uint32_t request_id) {
    std::vector<uint8_t> response;

    if (!wait_for_response(request_id, response, 10000)) {
        std::fprintf(stderr, "[Client] Timed out waiting for response\n");
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(g_response_slots_mtx);
        auto it = g_response_slots.find(request_id);
        if (it != g_response_slots.end()) {
            delete it->second;
            g_response_slots.erase(it);
        } else {
            std::cerr << "[receive_rpcresponse_internal] No slot found for request_id: " << request_id << "\n";
        }
    }

    uint32_t copy_len = std::min(max_len, static_cast<uint32_t>(response.size()));
    std::memcpy(dest, response.data(), copy_len);
    return copy_len;
}

int32_t receive_rpcresponse_internal_nonblocking(uint8_t* dest, uint32_t max_len, uint32_t request_id) {
    std::vector<uint8_t> response;

    {
        std::lock_guard<std::mutex> lock(g_response_slots_mtx);
        auto it = g_response_slots.find(request_id);
        if (it == g_response_slots.end()) {
            // Slot doesn't exist (shouldn’t happen in correct usage)
            std::cerr << "[receive_rpcresponse_internal] No slot found for request_id: " << request_id << "\n";
            return 0;
        }
        ResponseSlot* slot = it->second;
        // Non-blocking check: is the response ready?
        if (!slot->ready.load(std::memory_order_acquire)) {
            // Response not ready yet
            return 0;
        }
        // At this point, the response is ready — extract it
        {
            std::lock_guard<std::mutex> slot_lock(slot->mtx);
            response = std::move(slot->response);
        }
        // Clean up slot
        delete slot;
        g_response_slots.erase(it);
    }

    uint32_t copy_len = std::min(max_len, static_cast<uint32_t>(response.size()));
    std::memcpy(dest, response.data(), copy_len);
    return copy_len;
}