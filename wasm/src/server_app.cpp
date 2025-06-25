#include "rpc_server.h"
#include <string>
#include <vector>
#include <numeric> // for std::accumulate

extern "C" {
    void   rpc_server_ready();
}

// Simple business logic functions

std::string greet(int32_t id, std::string name) {
    // std::printf("[Server] Received message from ID %d: %s\n", id, name.c_str());
    // fflush(stdout);
    return "Hello back, " + name + "!";
}

int32_t add_constant(int32_t num) {
    return num + 42;
}

float sum_floats(std::vector<float> nums) {
    return std::accumulate(nums.begin(), nums.end(), 0.0f);
}

int main() {
    RpcServer server;

    // Register clean business logic handlers
    server.registerFunction(RpcEnvelope_msg_tag, greet);
    server.registerFunction(RpcEnvelope_rand_tag, add_constant);
    server.registerFunction(RpcEnvelope_flt_tag, sum_floats);
    server.registerBatchFunction(RpcEnvelope_batch_msg_tag, greet);
    fflush(stdout);

    rpc_server_ready(); // Notify the host that the server is ready

    // Process requests forever
    while (server.ProcessNextRequest()) {}

    return 0;
}
