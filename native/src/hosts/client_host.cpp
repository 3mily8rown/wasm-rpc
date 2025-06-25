#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <pthread.h>
#include <wasm_export.h>
#include <chrono>

#include "wamr/load_module.h"
#include "wasm/call_wasm.h"
#include "wamr/thread_launch.h"
#include "rpc/socket_communication.h"
#include "config.h"

#include "client_app_imports.h"
#include <unistd.h>
#include <iostream>

void register_all_env_symbols() {
    wasm_runtime_register_natives(
      "env",
      generated_client_app_native_symbols,
      generated_client_app_native_symbols_count
    );
}

int main() {
    // setting up wasm module
    wasm_runtime_set_log_level(WASM_LOG_LEVEL_DEBUG);
  
    // ------------------------------------------ per module
    wasm_module_t client_module = nullptr;
    wasm_module_inst_t client_module_inst = nullptr;
    wasm_exec_env_t client_exec_env = nullptr;

    // ------------------------------------------ wamr overall setup
    char error_buf[128];

    // debugger needs larger stack? Get Native stack overflow error when configured cmake with debug
    uint32_t buf_size, stack_size = 8092, heap_size = 8092;
  
    static char global_heap_buf[256 * 1024];
    RuntimeInitArgs init_args;
  
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Pool;
    
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);
  
    if (!wasm_runtime_full_init(&init_args)) {
      printf("Init runtime environment failed.\n");
      return -1;
    }
  
    wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);
  
    register_all_env_symbols();

    // ------------------------------------------------------- load each module
  
    // std::string client_wasm_path = Config::get("WASM_OUT") + "/client_app.wasm";
    std::string client_wasm_path = Config::get("WASM_OUT") + "/client_app.aot";
    auto client_buffer = readFileToBytes(client_wasm_path);
  
    // load module and create execution environment
    client_module = load_module_minimal(client_buffer, client_module_inst, client_exec_env, stack_size, heap_size, error_buf, sizeof(error_buf));
    if (!client_module) {
      return 1;
    }

    // ------------------------------
    // start socket listener
    pthread_t socket_thread;
    // pthread_create(&socket_thread, nullptr, [](void* arg) -> void* {
    //     auto* module_inst = static_cast<wasm_module_inst_t>(arg);
    //     socket_listener(response_port, INADDR_ANY);
    //     return nullptr;
    // }, client_module_inst);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64 * 1024);  // Or use THREAD_STACK_SIZE

    pthread_create(&socket_thread, &attr, [](void* arg) -> void* {
        socket_listener(response_port, INADDR_ANY);
        return nullptr;
    }, client_module_inst);

    pthread_attr_destroy(&attr);

    sleep(1);

    // ---------------------------------------------------------------- client

    wasm_runtime_set_log_level(WASM_LOG_LEVEL_ERROR);
    // calling client main function
    auto client_func = wasm_runtime_lookup_function(client_module_inst, "_start");
    if (!client_func) {
      fprintf(stderr, "_start wasm function is not found.\n");
      return 1;
    }

    // auto start = std::chrono::high_resolution_clock::now();

    pthread_t c_th;
    if (!start_wasm_thread(client_module_inst, client_func, 1, &c_th)) {
      std::fprintf(stderr, "Thread spawn failed\n");
    }

    // ------------------------------
    // wait for branches
    pthread_join(c_th, nullptr); 
    
    // auto end = std::chrono::high_resolution_clock::now();
    // double duration_us = std::chrono::duration<double, std::micro>(end - start).count();

    // std::cout << "[METRIC] Round-trip latency: " << duration_us << " us\n";
    // pthread_join(socket_thread, nullptr);  

    wasm_runtime_deinstantiate(client_module_inst);
    wasm_runtime_unload(client_module);
    wasm_runtime_destroy();

    return 0;
}