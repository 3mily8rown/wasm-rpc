#pragma once

#include <arpa/inet.h>
#include <cstdlib>

// Ports
constexpr int message_port = 12345;
constexpr int response_port = 12346;

// Default IP strings
constexpr const char* get_env_or_default(const char* env_var, const char* fallback) {
    const char* value = getenv(env_var);
    return value ? value : fallback;
}

inline const char* get_server_ip() {
    return get_env_or_default("SERVER_HOST", "127.0.0.1");
}

inline const char* get_client_ip() {
    return get_env_or_default("CLIENT_HOST", "127.0.0.1");
}


void socket_listener(int port = message_port, in_addr_t ip = INADDR_NONE);
void socket_response_listener(int port = response_port, in_addr_t ip = INADDR_NONE);

void send_over_socket(const uint8_t* data, uint32_t length, const char* ip, uint16_t port = message_port);
void send_response_over_socket(const uint8_t* data, uint32_t length, const char* ip, uint16_t port = response_port);

// Convenience overloads:
void send_over_socket(const uint8_t* data, uint32_t length); 
void send_response_over_socket(const uint8_t* data, uint32_t length, uint32_t request_id);

