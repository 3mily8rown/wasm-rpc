// rpc_transport.h
#pragma once

#include <cstdint>

// Client-side
int send_rpcmessage(void* ptr, int len);
int send_rpcmessage_with_id(void* ptr, int len, uint32_t id);
int receive_rpcresponse(void* ptr, int max_len);
int receive_rpcresponse_with_id(void* ptr, int max_len, uint32_t id);

// Server-side
int32_t receive_rpcmessage(void* ptr, uint32_t length);
void    send_rpcresponse(void* ptr, uint32_t length, uint32_t request_id);
