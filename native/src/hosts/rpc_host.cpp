#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <pthread.h>
#include <wasm_export.h>

#include "wamr/load_module.h"
#include "wasm/call_wasm.h"
#include "wamr/thread_launch.h"
#include "config.h"

#include "client_app_imports.h"
#include "server_app_imports.h"

struct WasmInstance {
    wasm_module_t module = nullptr;
    wasm_module_inst_t module_inst = nullptr;
    wasm_exec_env_t exec_env = nullptr;
    pthread_t thread;
};


bool init_wasm_instance(WasmInstance &inst, const std::string &path,
                        uint32_t stack_size, uint32_t heap_size,
                        char *error_buf, size_t error_buf_size) {
    auto buffer = readFileToBytes(path);
    inst.module = load_module_minimal(buffer, inst.module_inst, inst.exec_env,
                                      stack_size, heap_size, error_buf, error_buf_size);
    return inst.module != nullptr;
}

static const size_t all_env_native_symbols_count =
generated_client_app_native_symbols_count +
generated_server_app_native_symbols_count;

static NativeSymbol all_env_native_symbols[all_env_native_symbols_count];

void register_all_env_symbols() {
    size_t out_idx = 0;
    for (size_t i = 0; i < generated_client_app_native_symbols_count; ++i)
      all_env_native_symbols[out_idx++] = generated_client_app_native_symbols[i];
    for (size_t i = 0; i < generated_server_app_native_symbols_count; ++i)
      all_env_native_symbols[out_idx++] = generated_server_app_native_symbols[i];

    wasm_runtime_register_natives(
      "env",
      all_env_native_symbols,
      all_env_native_symbols_count
    );
}

int main() {
    wasm_runtime_set_log_level(WASM_LOG_LEVEL_DEBUG);

    // ------------------ WAMR global init ------------------
    static char global_heap_buf[8 * 1024 * 1024];  // increased heap
    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

    if (!wasm_runtime_full_init(&init_args)) {
        fprintf(stderr, "Init runtime environment failed.\n");
        return -1;
    }

    wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);
    register_all_env_symbols();

    colocated = true; // global flag

    // ------------------ Setup ------------------
    const int NUM_CLIENTS = 1;
    const int NUM_SERVERS = 1;
    uint32_t stack_size = 64 * 1024;
    uint32_t heap_size  = 128 * 1024;

    std::string client_path = Config::get("WASM_OUT") + "/client_app.aot";
    std::string server_path = Config::get("WASM_OUT") + "/server_app.aot";

    std::vector<WasmInstance> clients(NUM_CLIENTS);
    std::vector<WasmInstance> servers(NUM_SERVERS);

    char error_buf[128];

    // ------------------ Load Clients ------------------
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        auto buffer = readFileToBytes(client_path);
        clients[i].module = load_module_minimal(
            buffer,
            clients[i].module_inst,
            clients[i].exec_env,
            stack_size,
            heap_size,
            error_buf,
            sizeof(error_buf)
        );
        if (!clients[i].module) return 1;

        auto func = wasm_runtime_lookup_function(clients[i].module_inst, "_start");
        if (!func || !start_wasm_thread(clients[i].module_inst, func, i, &clients[i].thread)) {
            std::fprintf(stderr, "Failed to start client %d\n", i);
            return 1;
        }
    }

    // ------------------ Load Servers ------------------
    for (int i = 0; i < NUM_SERVERS; ++i) {
        auto buffer = readFileToBytes(server_path);
        servers[i].module = load_module_minimal(
            buffer,
            servers[i].module_inst,
            servers[i].exec_env,
            stack_size,
            heap_size,
            error_buf,
            sizeof(error_buf)
        );
        if (!servers[i].module) return 1;

        auto func = wasm_runtime_lookup_function(servers[i].module_inst, "_start");
        if (!func || !start_wasm_thread(servers[i].module_inst, func, i, &servers[i].thread)) {
            std::fprintf(stderr, "Failed to start server %d\n", i);
            return 1;
        }
    }

    // ------------------ Join Threads ------------------
    for (auto& c : clients)
        pthread_join(c.thread, nullptr);
    for (auto& s : servers)
        pthread_join(s.thread, nullptr);

    return 0;
}


// Note: The following code is commented out as it was part of an earlier version.
// int main2() {
//     // setting up wasm module
//     wasm_runtime_set_log_level(WASM_LOG_LEVEL_DEBUG);
  
//     // ------------------------------------------ per module
//     wasm_module_t client_module = nullptr;
//     wasm_module_inst_t client_module_inst = nullptr;
//     wasm_exec_env_t client_exec_env = nullptr;

//     wasm_module_t client_module2 = nullptr;
//     wasm_module_inst_t client_module_inst2 = nullptr;
//     wasm_exec_env_t client_exec_env2 = nullptr;

//     wasm_module_t server_module = nullptr;
//     wasm_module_inst_t server_module_inst = nullptr;
//     wasm_exec_env_t server_exec_env = nullptr;

//     wasm_module_t server_module2 = nullptr;
//     wasm_module_inst_t server_module_inst2 = nullptr;
//     wasm_exec_env_t server_exec_env2 = nullptr;

//     // ------------------------------------------ wamr overall setup
//     char error_buf[128];

//     // debugger needs larger stack? Get Native stack overflow error when configured cmake with debug
//     uint32_t buf_size, stack_size = 8092, heap_size = 8092;
  
//     static char global_heap_buf[2048 * 1024];
//     RuntimeInitArgs init_args;
  
//     memset(&init_args, 0, sizeof(RuntimeInitArgs));
//     init_args.mem_alloc_type = Alloc_With_Pool;
    
//     init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
//     init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);
  
//     if (!wasm_runtime_full_init(&init_args)) {
//       printf("Init runtime environment failed.\n");
//       return -1;
//     }
  
//     wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);
  
//     register_all_env_symbols();

//     // local consumer and client online flags
//     colocated = true;

//     // ------------------------------------------------------- load each module
  
//     std::string client_wasm_path = Config::get("WASM_OUT") + "/client_app.aot";
//     // std::string client_wasm_path = Config::get("WASM_OUT") + "/client_app.wasm";
//     auto client_buffer = readFileToBytes(client_wasm_path);
  
//     // load module and create execution environment
//     client_module = load_module_minimal(client_buffer, client_module_inst, client_exec_env, stack_size, heap_size, error_buf, sizeof(error_buf));
//     if (!client_module) {
//       return 1;
//     }

//     // second client
//     std::string client_wasm_path2 = Config::get("WASM_OUT") + "/client_app.aot";
//     // std::string client_wasm_path2 = Config::get("WASM_OUT") + "/client_app2.wasm";
//     auto client_buffer2 = readFileToBytes(client_wasm_path2);
  
//     // load module and create execution environment
//     client_module2 = load_module_minimal(client_buffer2, client_module_inst2, client_exec_env2, stack_size, heap_size, error_buf, sizeof(error_buf));
//     if (!client_module2) {
//       return 1;
//     }

//     // ----------------

//     std::string server_wasm_path = Config::get("WASM_OUT") + "/server_app.aot";
//     // std::string server_wasm_path = Config::get("WASM_OUT") + "/server_app.wasm";
//     auto server_buffer = readFileToBytes(server_wasm_path);
  
//     // load module and create execution environment
//     server_module = load_module_minimal(server_buffer, server_module_inst, server_exec_env, stack_size, heap_size, error_buf, sizeof(error_buf));
//     if (!server_module) {
//       return 1;
//     }

//     // ----------------second server

//     std::string server_wasm_path2 = Config::get("WASM_OUT") + "/server_app.aot";
//     // std::string server_wasm_path2 = Config::get("WASM_OUT") + "/server_app.wasm";
//     auto server_buffer2 = readFileToBytes(server_wasm_path2);
  
//     // load module and create execution environment
//     server_module2 = load_module_minimal(server_buffer2, server_module_inst2, server_exec_env2, stack_size, heap_size, error_buf, sizeof(error_buf));
//     if (!server_module2) {
//       return 1;
//     }

//     // ---------------------------------------------------------------- client

//     wasm_runtime_set_log_level(WASM_LOG_LEVEL_ERROR);
//     // calling client main function
//     auto client_func = wasm_runtime_lookup_function(client_module_inst, "_start");
//     if (!client_func) {
//       fprintf(stderr, "_start wasm function is not found.\n");
//       return 1;
//     }

//     pthread_t c_th;
//     if (!start_wasm_thread(client_module_inst, client_func, 1, &c_th)) {
//       std::fprintf(stderr, "Thread spawn failed\n");
//     }

//     // second client
//     // calling client main function
//     auto client_func2 = wasm_runtime_lookup_function(client_module_inst2, "_start");
//     if (!client_func2) {
//       fprintf(stderr, "_start wasm function is not found.\n");
//       return 1;
//     }

//     pthread_t c_th2;
//     if (!start_wasm_thread(client_module_inst2, client_func2, 3, &c_th2)) {
//       std::fprintf(stderr, "Thread spawn failed\n");
//     }

//     // ---------------------------------------------- server
//     auto server_func = wasm_runtime_lookup_function(server_module_inst, "_start");
//     if (!server_func) {
//       fprintf(stderr, "_start wasm function is not found.\n");
//       return 1;
//     }

//     pthread_t s_th;
//     if (!start_wasm_thread(server_module_inst, server_func, 2, &s_th)) {
//       std::fprintf(stderr, "Thread spawn failed\n");
//     }

//     // ---------------------------------------------- server
//     auto server_func2 = wasm_runtime_lookup_function(server_module_inst2, "_start");
//     if (!server_func2) {
//       fprintf(stderr, "_start wasm function is not found.\n");
//       return 1;
//     }

//     pthread_t s_th2;
//     if (!start_wasm_thread(server_module_inst2, server_func2, 0, &s_th2)) {
//       std::fprintf(stderr, "Thread spawn failed\n");
//     }

//     // ------------------------------
//     // wait for branches
     
//     pthread_join(c_th, nullptr);  
//     pthread_join(c_th2, nullptr);  

//     pthread_join(s_th, nullptr); 
//     pthread_join(s_th2, nullptr); 
//     return 0;
// }