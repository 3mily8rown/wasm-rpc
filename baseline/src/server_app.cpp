#include "rpc_server.h"
#include <string>
#include <vector>
#include <numeric> // for std::accumulate

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

void start_request_listener() {
    // This function should set up the environment to listen for incoming requests.
    // It could initialize network sockets, WASM memory, etc.
    // For this example, we assume it's a no-op.
    // In a real application, you would implement the necessary setup here.
    std::printf("[Server] Request listener started\n");
}

int main() {
    RpcServer server;

    // Register clean business logic handlers
    server.registerFunction(RpcEnvelope_msg_tag, greet);
    server.registerFunction(RpcEnvelope_rand_tag, add_constant);
    server.registerFunction(RpcEnvelope_flt_tag, sum_floats);
    server.registerBatchFunction(RpcEnvelope_batch_msg_tag, greet);

    // Process requests forever
    while (server.ProcessNextRequest()) {}

    return 0;
}
