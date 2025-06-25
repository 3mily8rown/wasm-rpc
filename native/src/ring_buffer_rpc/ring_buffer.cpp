#include "ring_buffer_rpc/ring_buffer.h"
#include <cstring>
#include <stdexcept>

RingBuffer::RingBuffer(size_t capacity)
  : _capacity(capacity),
    _buffer(new uint8_t[capacity]),
    _head(0),
    _tail(0)
{
    if (capacity < sizeof(uint32_t) + 1) {
        throw std::invalid_argument("Capacity too small for ring buffer");
    }
}

RingBuffer::~RingBuffer() {
    delete[] _buffer;
}

bool RingBuffer::enqueue(const uint8_t* data, uint32_t length) {
    uint32_t total = sizeof(uint32_t) + length;
    if (total > _capacity - 1) {
        return false;
    }

    std::unique_lock<std::mutex> lock(_mutex);

    uint32_t free_space = calculate_free_space_locked();
    if (free_space < total) {
        return false;
    }

    uint8_t hdr[4];
    hdr[0] = uint8_t((length >>  0) & 0xFF);
    hdr[1] = uint8_t((length >>  8) & 0xFF);
    hdr[2] = uint8_t((length >> 16) & 0xFF);
    hdr[3] = uint8_t((length >> 24) & 0xFF);

    _head = copy_into_ring(_head, hdr, 4);
    _head = copy_into_ring(_head, data, length);

    lock.unlock();
    _cv.notify_one();
    return true;
}

uint32_t RingBuffer::dequeue(uint8_t* dest, uint32_t max_length) {
    std::unique_lock<std::mutex> lock(_mutex);

    _cv.wait(lock, [&]() { return _head != _tail; });

    uint8_t hdr[4];
    uint32_t next = copy_from_ring(_tail, hdr, 4);
    uint32_t msg_len =
         (uint32_t)hdr[0]
       | ((uint32_t)hdr[1] << 8)
       | ((uint32_t)hdr[2] << 16)
       | ((uint32_t)hdr[3] << 24);

    if (msg_len > _capacity - sizeof(uint32_t)) {
        throw std::runtime_error("RingBuffer::dequeue: invalid message length");
    }

    uint32_t total_size = sizeof(uint32_t) + msg_len;
    _cv.wait(lock, [&]() { return calculate_used_space_locked() >= total_size; });

    uint32_t to_copy = (msg_len <= max_length ? msg_len : max_length);
    next = copy_from_ring(next, dest, to_copy);

    uint32_t skip = msg_len - to_copy;
    if (skip > 0) {
        if (next + skip < _capacity) {
            next += skip;
        } else {
            next = (next + skip) - _capacity;
        }
    }

    _tail = next;

    return msg_len;
}

int32_t RingBuffer::dequeue_with_timeout(uint8_t* dest, uint32_t max_length, int timeout_ms) {
    std::unique_lock<std::mutex> lock(_mutex);

    if (!_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() { return _head != _tail; })) {
        return -1;
    }

    uint8_t hdr[4];
    uint32_t next = copy_from_ring(_tail, hdr, 4);
    uint32_t msg_len =
         (uint32_t)hdr[0]
       | ((uint32_t)hdr[1] << 8)
       | ((uint32_t)hdr[2] << 16)
       | ((uint32_t)hdr[3] << 24);

    if (msg_len > _capacity - sizeof(uint32_t)) {
        throw std::runtime_error("RingBuffer::dequeue_with_timeout: invalid message length");
    }

    uint32_t total_size = sizeof(uint32_t) + msg_len;
    if (!_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() { return calculate_used_space_locked() >= total_size; })) {
        return -1;
    }

    uint32_t to_copy = (msg_len <= max_length ? msg_len : max_length);
    next = copy_from_ring(next, dest, to_copy);

    uint32_t skip = msg_len - to_copy;
    if (skip > 0) {
        if (next + skip < _capacity) {
            next += skip;
        } else {
            next = (next + skip) - _capacity;
        }
    }

    _tail = next;
    return msg_len;
}

uint32_t RingBuffer::calculate_free_space_locked() const {
    if (_head >= _tail) {
        return uint32_t(_capacity - (_head - _tail) - 1);
    } else {
        return uint32_t((_tail - _head) - 1);
    }
}

uint32_t RingBuffer::calculate_used_space_locked() const {
    if (_head >= _tail) {
        return uint32_t(_head - _tail);
    } else {
        return uint32_t(_capacity - (_tail - _head));
    }
}

uint32_t RingBuffer::copy_into_ring(uint32_t idx, const uint8_t* src, uint32_t n) {
    if (idx + n <= _capacity) {
        std::memcpy(&_buffer[idx], src, n);
        idx += n;
        if (idx == _capacity) idx = 0;
        return idx;
    } else {
        uint32_t first_part = uint32_t(_capacity - idx);
        std::memcpy(&_buffer[idx], src, first_part);
        uint32_t second_part = n - first_part;
        std::memcpy(&_buffer[0], src + first_part, second_part);
        return second_part;
    }
}

uint32_t RingBuffer::copy_from_ring(uint32_t idx, uint8_t* dst, uint32_t n) const {
    if (idx + n <= _capacity) {
        std::memcpy(dst, &_buffer[idx], n);
        idx += n;
        if (idx == _capacity) idx = 0;
        return idx;
    } else {
        uint32_t first_part = uint32_t(_capacity - idx);
        std::memcpy(dst, &_buffer[idx], first_part);
        uint32_t second_part = n - first_part;
        std::memcpy(dst + first_part, &_buffer[0], second_part);
        return second_part;
    }
}
