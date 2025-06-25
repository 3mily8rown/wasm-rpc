#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <iostream>

// Used by both client (to wait) and server (to deliver)
struct ResponseSlot {
    std::vector<uint8_t> response;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> ready = false;

    // Waits for data or times out
    bool wait_for_data(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, timeout, [&]() {
            // std::cout << "[ResponseSlot] Waiting for data... and ready: " << ready.load(std::memory_order_acquire) << "\n";
            return ready.load(std::memory_order_acquire);
        });
    }

};

struct SyncResponseSlot {
    std::uint8_t ptr;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> ready = false;

    // Waits for data or times out
    bool wait_for_data(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, timeout, [&]() {
            return ready.load(std::memory_order_acquire);
        });
    }
};

// Globals (defined in rpc_messaging.cpp)
extern std::unordered_map<uint32_t, ResponseSlot*> g_response_slots;
extern std::mutex g_response_slots_mtx;

// Core API
void queue_message(const uint8_t* data, uint32_t length);
uint32_t dequeue_message(uint8_t* dest, uint32_t max_length);
int32_t dequeue_message_with_timeout(uint8_t* dest, uint32_t max_length, int timeout_ms);

void begin_wait_for_response(uint32_t request_id);
bool wait_for_response(uint32_t request_id, std::vector<uint8_t>& out, int timeout_ms);
void deliver_response(uint32_t request_id, const uint8_t* data, uint32_t len);
