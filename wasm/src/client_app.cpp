#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include "rpc_client.h"
#include "rpc_envelope.pb.h"
#include <iostream>
#include <unordered_set>
#include <time.h>


extern "C" {
    int64_t get_time_us();
    void send_rtt(uint32_t time_us);
    void send_total(uint32_t time_us, uint32_t count);
}

// main function implemenations for evaluation purposes
int send_x_messages(int count) {
    RpcClient client;

    int64_t initial_time = get_time_us();
    for (int i = 0; i < count; i++) {
        int64_t t0 = get_time_us();

        std::string ack = client.sendMessage(i, "hello from client");
        if (ack.empty()) {
            std::fprintf(stderr, "Failed to send message\n");
            return 1;
        }

        int64_t t1 = get_time_us();
        send_rtt(static_cast<uint32_t>(t1 - t0));
    }

    send_total(static_cast<uint32_t>(get_time_us() - initial_time), count);
    return 0;
}

int send_x_messages_no_rtt(int count) {
    RpcClient client;

    int64_t initial_time = get_time_us();
    for (int i = 0; i < count; i++) {
        std::string ack = client.sendMessage(i, "hello from client");
        if (ack.empty()) {
            std::fprintf(stderr, "Failed to send message\n");
            return 1;
        }
    }
    send_total(static_cast<uint32_t>(get_time_us() - initial_time), count);
    return 0;
}

int send_add_random(int count) {
    RpcClient client;

    int64_t initial_time = get_time_us();
    for (int i = 0; i < count; i++) {
        int64_t t0 = get_time_us();

        int32_t result = client.addRandom(i);
        if (result < 0) {
            std::fprintf(stderr, "addRandom failed\n");
            return 1;
        }

        int64_t t1 = get_time_us();
        send_rtt(static_cast<uint32_t>(t1 - t0));
    }

    send_total(static_cast<uint32_t>(get_time_us() - initial_time), count);
    return 0;
}

int send_process_floats(int count) {
    RpcClient client;

    int64_t initial_time = get_time_us();
    for (int i = 0; i < count; i++) {
        std::vector<float> nums = {1.0f * i, 2.0f * i, 3.0f * i};

        int64_t t0 = get_time_us();

        float result = client.processFloats(nums);
        if (result < 0.0f) {
            std::fprintf(stderr, "processFloats failed\n");
            return 1;
        }

        int64_t t1 = get_time_us();
        send_rtt(static_cast<uint32_t>(t1 - t0));
    }

    send_total(static_cast<uint32_t>(get_time_us() - initial_time), count);
    return 0;
}

int send_varied_messages(int count) {
    RpcClient client;

    for (int i = 8; i < 2048; i *= 2) {
        char name_buf[sizeof(SendMessage::name)] = {};
        int fill_len = std::min(i, static_cast<int>(sizeof(name_buf)) - 1);
        std::memset(name_buf, 'A', fill_len);
        name_buf[fill_len] = '\0';

        int64_t initial_time = get_time_us();
        for (int j = 0; j < count; j++) {
            int64_t t0 = get_time_us();

            std::string ack = client.sendMessage(j, name_buf);
            if (ack.empty()) {
                std::fprintf(stderr, "Failed to send message\n");
                return 1;
            }

            int64_t t1 = get_time_us();
            send_rtt(static_cast<uint32_t>(t1 - t0));
        }

        send_total(static_cast<uint32_t>(get_time_us() - initial_time), count);
    }

    return 0;
}

int send_async_messages_test(int count) {
    RpcClient client;
    std::unordered_set<uint32_t> pending_ids;

    int64_t start_time = get_time_us();

    // 1. Send all messages
    for (int i = 0; i < count; ++i) {
        std::string msg = "hello from client " + std::to_string(i);
        uint32_t id = client.sendMessageAsync(i, msg.c_str());
        if (id == 0) {
            std::fprintf(stderr, "Failed to send message\n");
            return 1;
        }
        pending_ids.insert(id);
    }

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50 * 1000; // 50 microseconds

    // 2. Poll until all responses received
    std::string result;
    while (!pending_ids.empty()) {
        std::vector<uint32_t> completed;

        for (uint32_t id : pending_ids) {
            if (client.pollSendMessageResponse(id, result)) {
                completed.push_back(id);
            }
        }

        for (uint32_t id : completed) {
            pending_ids.erase(id);
        }

        if (completed.empty()) {
            nanosleep(&ts, nullptr);
        }
    }

    int64_t total_time = get_time_us() - start_time;
    send_total(static_cast<uint32_t>(total_time), count);
    return 0;
}


int send_batch_messages(int count) {
    RpcClient client;

    std::vector<std::string> messages;
    for (int i = 0; i < count; i++) {
        messages.push_back("Batch message " + std::to_string(i));
    }

    std::string ack = client.sendMessage(0, "Batch message start");
    std::fprintf(stdout, "Batch start ack: %s\n", ack.c_str());
    fflush(stdout);

    int64_t initial_time = get_time_us();

    uint32_t batch_id = client.sendMessageBatch(messages);
    if (batch_id == 0) {
        std::fprintf(stderr, "Failed to send batch message\n");
        return 1;
    }

    std::unordered_map<uint32_t, std::string> results;
    if (!client.waitForBatchResponse(batch_id, results)) {
        std::fprintf(stderr, "Failed to receive batch response\n");
        return 1;
    }

    for (const auto& [id, msg] : results) {
        std::fprintf(stdout, "Batch response for ID %u: %s\n", id, msg.c_str());
    }

    send_total(static_cast<uint32_t>(get_time_us() - initial_time), count);
    return 0;
}



int main() {
    // resources test
    // send_x_messages_no_rtt(10000);

    // round trip time tests
    // send_x_messages(10000);

    // asynchronous message tests
    send_async_messages_test(10000);

    // send_batch_messages(3);
    // send_add_random(5);
    // send_process_floats(5);
    // send_varied_messages(10);
    return 0;
}
