#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <netinet/in.h>

class PersistentSocketClient {
public:
    PersistentSocketClient(const std::string& ip, uint16_t port);
    ~PersistentSocketClient();

    bool connect();
    bool sendMessage(const uint8_t* data, uint32_t length);
    void disconnect();

private:
    bool send_all(const uint8_t* data, size_t len);

    std::string ip_;
    uint16_t port_;
    int sock_;
    std::mutex mutex_;
};
