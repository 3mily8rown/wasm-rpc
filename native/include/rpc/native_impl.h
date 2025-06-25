// native_impls.h
#pragma once

#include <cstdint>
#include <wasm_export.h>   // for wasm_exec_env_t, wasm_module_inst_t, etc.
#include <string>

extern bool colocated;

void rpc_server_ready(wasm_exec_env_t exec_env);

int32_t send_rpcmessage(wasm_exec_env_t exec_env, uint32_t offset,
    uint32_t length);

int32_t receive_rpcmessage(wasm_exec_env_t exec_env, uint32_t offset, 
    uint32_t max_length);

int32_t receive_rpcresponse(wasm_exec_env_t exec_env, uint32_t offset, 
    uint32_t max_len);

void send_rpcresponse(wasm_exec_env_t exec_env, uint32_t offset, 
    uint32_t length, uint32_t request_id);

// Asynchronous client support

int32_t send_rpcmessage_with_id(wasm_exec_env_t exec_env, uint32_t offset,
    uint32_t length, uint32_t request_id);

int32_t receive_rpcresponse_with_id(wasm_exec_env_t exec_env, uint32_t offset, 
    uint32_t max_len, uint32_t request_id);

// Time-related functions for evaluations
int64_t get_time_us(wasm_exec_env_t exec_env);

void send_rtt(wasm_exec_env_t exec_env, uint32_t time_us);

void send_total(wasm_exec_env_t exec_env, uint32_t time_us, uint32_t count);