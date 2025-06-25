#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <condition_variable>

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer();

    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Enqueue a message with payload length `length`.
    // Returns true if message was enqueued successfully.
    bool enqueue(const uint8_t* data, uint32_t length);

    // Blocking dequeue: waits until a complete message is available.
    // Copies up to `max_length` bytes into `dest`.
    // Returns actual message length (even if > max_length).
    uint32_t dequeue(uint8_t* dest, uint32_t max_length);

    // Timeout variant: returns -1 if timeout occurs before full message is available.
    int32_t dequeue_with_timeout(uint8_t* dest, uint32_t max_length, int timeout_ms);

private:
    const size_t _capacity;
    uint8_t* _buffer;
    uint32_t _head;
    uint32_t _tail;

    std::mutex _mutex;
    std::condition_variable _cv;

    // Helpers (all assume _mutex is held)
    uint32_t calculate_free_space_locked() const;
    uint32_t calculate_used_space_locked() const;
    uint32_t copy_into_ring(uint32_t idx, const uint8_t* src, uint32_t n);
    uint32_t copy_from_ring(uint32_t idx, uint8_t* dst, uint32_t n) const;
};

#endif // RING_BUFFER_H
