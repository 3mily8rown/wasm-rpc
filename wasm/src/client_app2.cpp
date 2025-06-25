#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include "rpc_client.h"
#include "rpc_envelope.pb.h"

extern "C" {
    int64_t get_time_us();
    void send_rtt(uint32_t time_us);
    void send_total(uint32_t time_us, uint32_t count);
}

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

int main() {
    send_process_floats(5);
    return 0;
}
