#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <pthread.h>
#include <wasm_export.h>
#include <iostream>
#include "wamr/load_module.h"
#include "wasm/call_wasm.h"
#include "wamr/thread_launch.h"
#include "rpc/socket_communication.h"
#include "config.h"

#include "server_app_imports.h"
#include <unistd.h>

void register_all_env_symbols() {

    wasm_runtime_register_natives(
      "env",
      generated_server_app_native_symbols,
      generated_server_app_native_symbols_count
    );
}

int main() {
    std::cerr << "[Server] starting up\n";          // unbuffered

    // setting up wasm module
    wasm_runtime_set_log_level(WASM_LOG_LEVEL_DEBUG);
  
    // ------------------------------------------ per module

    wasm_module_t server_module = nullptr;
    wasm_module_inst_t server_module_inst = nullptr;
    wasm_exec_env_t server_exec_env = nullptr;

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
      std::cerr << "[Server] Init runtime environment failed." << std::endl;
      return -1;
    }
  
    wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);
  
    register_all_env_symbols();

    // ------------------------------------------------------- load each module

    std::string server_wasm_path = Config::get("WASM_OUT") + "/server_app.aot";
    // std::string server_wasm_path = Config::get("WASM_OUT") + "/server_app.wasm";
    auto server_buffer = readFileToBytes(server_wasm_path);
  
    // load module and create execution environment
    server_module = load_module_minimal(server_buffer, server_module_inst, server_exec_env, stack_size, heap_size, error_buf, sizeof(error_buf));
    if (!server_module) {
      std::cerr << "[Server] Failed to load WASM module: " 
                  << error_buf << std::endl;
      return 1;
    }

    // ------------------------------
    // start socket listener
    std::cout << "[Server] start socket listener." << std::endl;
    pthread_t socket_thread;
    // pthread_create(&socket_thread, nullptr, [](void* arg) -> void* {
    //     auto* module_inst = static_cast<wasm_module_inst_t>(arg);
    //     socket_listener(message_port);
    //     return nullptr;
    // }, server_module_inst);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64 * 1024);  // Or use THREAD_STACK_SIZE

    pthread_create(&socket_thread, &attr, [](void* arg) -> void* {
        socket_listener(message_port, INADDR_ANY);
        return nullptr;
    }, server_module_inst);

    pthread_attr_destroy(&attr);

    sleep(1);

    // ---------------------------------------------- server
    std::cout << "[Server] find server func" << std::endl;
    auto server_func = wasm_runtime_lookup_function(server_module_inst, "_start");
    if (!server_func) {
      fprintf(stderr, "_start wasm function is not found.\n");
      return 1;
    }

    std::cout << "[Server] start server" << std::endl;
    pthread_t s_th;
    if (!start_wasm_thread(server_module_inst, server_func, 0, &s_th)) {
      std::fprintf(stderr, "Thread spawn failed\n");
    }


    // ----------------------------------
    std::cout << "[Server] wait for threads" << std::endl;

    // wait for branches
    pthread_join(s_th, nullptr);  
    // pthread_join(socket_thread, nullptr);  

    wasm_runtime_deinstantiate(server_module_inst);
    wasm_runtime_unload(server_module);
    wasm_runtime_destroy();

    return 0;
}