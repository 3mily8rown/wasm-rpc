// ring_buffer_rpc/rpc_messaging.cpp

#include "ring_buffer_rpc/rpc_messaging.h"
#include "ring_buffer_rpc/ring_buffer.h"
#include <cstdint>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <unordered_map>

// ------------------------------------------------------------------------------------------------------------
// global ring buffer (requests) and map of response slots
// ------------------------------------------------------------------------------------------------------------
static constexpr size_t REQUEST_BUFFER_CAPACITY  = 1 * 1024 * 1024; // 1 MiB
static RingBuffer g_request_rb (REQUEST_BUFFER_CAPACITY);

std::unordered_map<uint32_t, ResponseSlot*> g_response_slots;
std::mutex g_response_slots_mtx;

// ------------------------------------------------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------------------------------------------------

// Enqueue a request. If it doesn't fit, print an error.
void queue_message(const uint8_t* data, uint32_t length) {
    // Check “never‐fit” condition first
    if (uint64_t(length) + sizeof(uint32_t) > REQUEST_BUFFER_CAPACITY - 1) {
        std::cerr << "[RingBuffer] queue_message: message too large ("
                  << length << " bytes)\n";
        return;
    }
    bool ok = g_request_rb.enqueue(data, length);
    if (!ok) {
        std::cerr << "[RingBuffer] queue_message: not enough space for "
                  << length << "‐byte message\n";
    }
}

// Dequeue exactly one request. BLOCKS until a complete message is available.
// Returns the actual payload length (caller can compare against max_length
// to see if truncation occurred, but in normal operation payload <= max_length).
uint32_t dequeue_message(uint8_t* dest, uint32_t max_length) {
    return g_request_rb.dequeue(dest, max_length);
}

int32_t dequeue_message_with_timeout(uint8_t* dest, uint32_t max_length, int timeout_ms) {
    return g_request_rb.dequeue_with_timeout(dest, max_length, timeout_ms);
}

void begin_wait_for_response(uint32_t request_id) {
    // std::cout << "[begin_wait_for_response] Request ID: " << request_id << "\n";
    std::lock_guard<std::mutex> lock(g_response_slots_mtx);
    if (g_response_slots.count(request_id)) {
        std::cerr << "[begin_wait_for_response] Duplicate request_id: " << request_id << "\n";
        return; // or overwrite only if you're sure that's safe
    }
    g_response_slots[request_id] = new ResponseSlot();
}


bool wait_for_response(uint32_t request_id, std::vector<uint8_t>& out, int timeout_ms) {
    ResponseSlot* slot;

    {
        std::lock_guard<std::mutex> lock(g_response_slots_mtx);
        auto it = g_response_slots.find(request_id);
        if (it == g_response_slots.end()) {
            std::cerr << "[wait_for_response] Unknown request_id " << request_id << "\n";
            return false;
        }
        slot = it->second;
    }

    if (!slot->wait_for_data(std::chrono::milliseconds(timeout_ms))) {
        std::cerr << "[wait_for_response] Timeout on slot " << slot << "\n";
        return false;
    }

    out = std::move(slot->response);
    return true;
}


void deliver_response(uint32_t request_id, const uint8_t* data, uint32_t len) {
    ResponseSlot* slot = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_response_slots_mtx);
        auto it = g_response_slots.find(request_id);
        if (it == g_response_slots.end()) {
            std::cerr << "[deliver_response] Unknown request_id " << request_id << "\n";
            return;
        }

        slot = it->second;
    }

    {
        std::lock_guard<std::mutex> slot_lock(slot->mtx);
        slot->response.assign(data, data + len);
        slot->ready.store(true, std::memory_order_release);
    }

    slot->cv.notify_one();  // notify after unlocking global and slot mutex
}


void remove_response_slot(uint32_t request_id) {
    std::lock_guard<std::mutex> lock(g_response_slots_mtx);
    auto it = g_response_slots.find(request_id);
    if (it != g_response_slots.end()) {
        delete it->second;
        g_response_slots.erase(it);
        std::cerr << "[remove_response_slot] Removed slot for request_id " << request_id << "\n";
    }
}
