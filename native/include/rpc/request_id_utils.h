// rpc/rpc_id_utils.h

#pragma once
#include <cstdint>

inline uint32_t make_full_id(uint16_t local_id, uint16_t thread_id) {
    return (static_cast<uint32_t>(local_id) << 16) | thread_id;
}

inline uint16_t extract_thread_id(uint32_t full_id) {
    return static_cast<uint16_t>(full_id & 0xFFFF);
}

inline uint16_t extract_local_id(uint32_t full_id) {
    return static_cast<uint16_t>(full_id >> 16);
}
