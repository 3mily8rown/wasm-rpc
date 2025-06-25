#pragma once

#include <pthread.h>
#include <wasm_export.h>

#include "wasm/call_wasm.h"  

uint32_t get_thread_id();

bool start_wasm_thread(WASMModuleInstanceCommon *module_inst,
                    wasm_function_inst_t func,
                    uint32_t thread_id,
                    pthread_t *out_thread);