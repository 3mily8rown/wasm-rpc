#include <iostream>
#include <cstdint>
#include <array>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <queue>
#include <vector>
#include <wasm_export.h>
#include<unistd.h>
#include <atomic>

#include "ring_buffer_rpc/rpc_messaging.h"
#include "rpc/socket_communication.h"
#include "rpc/native_impl.h"
#include "wamr/thread_launch.h"
#include "rpc/request_id_utils.h"
#include <execinfo.h>

std::atomic<bool> g_local_consumer_online = false;
std::atomic<bool> g_local_client_online = false;
std::atomic<bool> g_server_ready = false;
bool colocated = false;

// Function prototypes for internal use
int32_t send_rpcmessage_internal(uint8_t* src, uint32_t length, uint32_t request_id);
int32_t receive_rpcresponse_internal(uint8_t* dest, uint32_t max_len, uint32_t request_id);
int32_t receive_rpcresponse_internal_nonblocking(uint8_t* dest, uint32_t max_len, uint32_t request_id);

void rpc_server_ready(wasm_exec_env_t env) {
    g_server_ready.store(true, std::memory_order_release);
}

// sync
int32_t send_rpcmessage(wasm_exec_env_t exec_env, uint32_t offset, uint32_t length) {
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    uint8_t* src = static_cast<uint8_t*>(wasm_runtime_addr_app_to_native(inst, offset));

    if (!src) {
        printf("Invalid memory address\n");
        return -1;
    }
    if (length < 4) {
        printf("Buffer too small for request_id prefix\n");
        return -1;
    }

    uint32_t request_id = get_thread_id();
    begin_wait_for_response(request_id);

    return send_rpcmessage_internal(src, length, request_id);
}

int32_t receive_rpcmessage(wasm_exec_env_t exec_env, uint32_t offset, uint32_t max_length) {
    if (!g_server_ready.load(std::memory_order_acquire)) {
        std::fprintf(stderr,
            "[WARN] blocked early receive/send\n");
        return 0;         // or just return;  for void function
    }

    extern std::atomic<bool> g_local_consumer_online;
    g_local_consumer_online.store(true, std::memory_order_release);

    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    uint8_t* dest = static_cast<uint8_t*>(wasm_runtime_addr_app_to_native(inst, offset));
    if (!dest) {
        printf("Invalid memory address\n");
        return 0;
    }

    return dequeue_message_with_timeout(dest, max_length, 10000);
}

void send_rpcresponse(wasm_exec_env_t exec_env, uint32_t offset, uint32_t length, uint32_t request_id) {
    if (!g_server_ready.load(std::memory_order_acquire)) {
        std::fprintf(stderr,
            "[WARN] blocked early receive/send\n");
        return;         // or just return;  for void function
    }


    // void* callstack[10];
    // int frames = backtrace(callstack, 10);
    // char** strs = backtrace_symbols(callstack, frames);
    // std::cerr << "send_rpcresponse backtrace:\n";
    // for (int i = 0; i < frames; ++i)
    //     std::cerr << strs[i] << "\n";
    // free(strs);
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    uint8_t* src = static_cast<uint8_t*>(wasm_runtime_addr_app_to_native(inst, offset));

    if (!src) {
        printf("Invalid memory address\n");
        return;
    }
    if (g_local_client_online.load(std::memory_order_acquire)) {
        // printf("Local client is online, sending message...\n");
        deliver_response(request_id , src, length);
    } else if (colocated) {
        // printf("Colocated environment detected, client not ready...\n");
        deliver_response(request_id , src, length);
    } else {
        // printf("Local client is offline, sending over socket...\n");
        send_response_over_socket(src, length, request_id);
    }    
}




int32_t receive_rpcresponse(wasm_exec_env_t exec_env, uint32_t offset, uint32_t max_len) {
    extern std::atomic<bool> g_local_client_online;
    g_local_client_online.store(true, std::memory_order_release);

    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    uint8_t* dest = static_cast<uint8_t*>(wasm_runtime_addr_app_to_native(inst, offset));
    if (!dest) {
        printf("Invalid memory address\n");
        return 0;
    }

    uint32_t request_id = get_thread_id();

    return receive_rpcresponse_internal(dest, max_len, request_id);
}

// Asynchronous RPC functions - to handle multiple outstanding requests per client
int32_t send_rpcmessage_with_id(wasm_exec_env_t exec_env, uint32_t offset,
    uint32_t length, uint32_t local_id) {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
        uint8_t* src = static_cast<uint8_t*>(wasm_runtime_addr_app_to_native(inst, offset));

        if (!src) {
            printf("Invalid memory address\n");
            return -1;
        }
        if (length < 4) {
            printf("Buffer too small for request_id prefix\n");
            return -1;
        }

        uint32_t full_id = make_full_id(local_id, get_thread_id());
        begin_wait_for_response(full_id);

        return send_rpcmessage_internal(src, length, full_id);
}

int32_t receive_rpcresponse_with_id(wasm_exec_env_t exec_env, uint32_t offset, 
    uint32_t max_len, uint32_t local_id) {
        extern std::atomic<bool> g_local_client_online;
        g_local_client_online.store(true, std::memory_order_release);

        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
        uint8_t* dest = static_cast<uint8_t*>(wasm_runtime_addr_app_to_native(inst, offset));
        if (!dest) {
            printf("Invalid memory address\n");
            return 0;
        }

        return receive_rpcresponse_internal_nonblocking(dest, max_len, make_full_id(local_id, get_thread_id()));
}

// private helper functions

int32_t send_rpcmessage_internal(uint8_t* src, uint32_t length, uint32_t request_id) {
    // Add request_id to your message frame
    std::memcpy(src, &request_id, sizeof(uint32_t));

    if (g_local_consumer_online.load(std::memory_order_acquire)) {
        queue_message(src, length);
    } else if (colocated) {
        queue_message(src, length);
    } else {
        // printf("Local server is offline, sending over socket...\n");
        send_over_socket(src, length);
    }    
    return 0;
}

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

// ----------------------------------------------------------
// Metrics functions

int64_t get_time_us(wasm_exec_env_t exec_env) {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    return duration_cast<microseconds>(now.time_since_epoch()).count();
}

void send_rtt(wasm_exec_env_t exec_env, uint32_t time_us) {
    std::cout << "[METRICS] RTT = " << time_us << "μs" << std::endl;
}

void send_total(wasm_exec_env_t exec_env, uint32_t time_us, uint32_t count) {
    std::cout << "[METRICS] TOTAL TIME = " << time_us << "μs for size " << count << std::endl;
    // uint32_t throughput = (count > 0) ? (time_us / count) : 0;
    // std::cout << "[METRICS] THROUGHPUT = " << throughput << "μs" << std::endl;
}
