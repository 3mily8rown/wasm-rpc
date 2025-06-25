#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <iostream>

#include "wasm/call_wasm.h"
#include "wamr/thread_launch.h"

#define THREAD_STACK_SIZE (64 * 1024)

typedef struct {
    WASMModuleInstanceCommon *module_inst;
    wasm_function_inst_t       func;
    uint32_t thread_id;
} ThreadArg;

thread_local uint32_t tls_thread_id = 0;

// Assumes func takes in no args, expects no returns
static void *wasm_thread_entry(void *raw_arg) {
    ThreadArg *arg = static_cast<ThreadArg*>(raw_arg);

    // Initialize thread environment
    if (!wasm_runtime_init_thread_env()) {
        std::fprintf(stderr, "failed to init WAMR thread env\n");
        delete arg;
        return nullptr;
    }

    // Create execution environment
    WASMExecEnv *exec_env =
        wasm_runtime_create_exec_env(arg->module_inst, THREAD_STACK_SIZE);
    if (!exec_env) {
        std::fprintf(stderr, "failed to create exec env for thread\n");
        wasm_runtime_destroy_thread_env();
        delete arg;
        return nullptr;
    }

    // Here you can store or use arg->thread_id as needed,
    // for example set thread-local variable to identify this thread's client ID
    tls_thread_id = arg->thread_id;
    std::cout << "[WASM Thread] Started with ID: " << tls_thread_id << "\n";

    // Call WASM function
    if (!call_no_args(exec_env, arg->module_inst, arg->func)) {
        const char *ex = wasm_runtime_get_exception(arg->module_inst);
        std::cout << "[WASM Thread] thread " << tls_thread_id
                  << " failed to call function: " << (ex ? ex : "(null)") << "\n";
        std::fprintf(stderr,
                     "wasm thread failed: %s\n",
                     ex ? ex : "(null)");
    }

    // Clean up
    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_destroy_thread_env();
    delete arg;
    return nullptr;
}

uint32_t get_thread_id() {
    return tls_thread_id ? tls_thread_id : 0; // Return 0 if not set
}

// Now accepts thread_id so caller can assign a unique client ID per thread
bool start_wasm_thread(WASMModuleInstanceCommon *module_inst,
                       wasm_function_inst_t func,
                       uint32_t thread_id,
                       pthread_t *out_thread) {

    ThreadArg *arg = new ThreadArg{ module_inst, func, thread_id };
    if (!arg) {
        std::fprintf(stderr, "out of memory when allocating ThreadArg\n");
        return false;
    }

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // Set smaller stack (e.g., 64 KB instead of default 2 MB)
    pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

    int err = pthread_create(&tid, &attr, wasm_thread_entry, arg);
    pthread_attr_destroy(&attr); // optional cleanup
    // int err = pthread_create(&tid, nullptr, wasm_thread_entry, arg);
    if (err != 0) {
        std::fprintf(stderr,
                     "pthread_create failed: %s\n",
                     strerror(err));
        delete arg;
        return false;
    }

    if (out_thread) {
        *out_thread = tid;
    } else {
        pthread_detach(tid);
    }

    return true;
}
