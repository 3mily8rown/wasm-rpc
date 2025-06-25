#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <thread>

// #include "rpc/message_queue.h"
#include "ring_buffer_rpc/rpc_messaging.h"
#include "rpc/socket_communication.h"

#include <unordered_map>
#include <mutex>

static std::mutex sock_mutex;
static std::unordered_map<uint16_t, int> persistent_sockets;


static constexpr int MAX_CONNECT_ATTEMPTS = 5;
static constexpr int MAX_SEND_ATTEMPTS    = 3;
static constexpr int INITIAL_BACKOFF_MS   = 100;

in_addr_t resolve_ip_or_throw(const char* hostname);


void socket_listener(int port, in_addr_t ip) {
    std::cout << "[Native] Starting socket going to listen on port " << port << "\n";

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Native] socket");
        exit(1);
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    char ip_buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf));
    std::cout << "[Native] Binding to " << ip_buf << ":" << port << "\n";

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[Native] bind");
        exit(1);
    }

    if (listen(server_fd, 1) < 0) {
        perror("[Native] listen");
        exit(1);
    }

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("[Native] accept");
            continue;
        }

        // Read length-prefixed messages
        while (true) {
            uint32_t msg_len = 0;
            ssize_t r = recv(client_fd, &msg_len, sizeof(msg_len), MSG_WAITALL);
            if (r == 0) break;  // client closed connection
            if (r < 0) {
                perror("[Native] recv (length)");
                break;
            }
            if (r != sizeof(msg_len)) {
                std::cerr << "[Native] Failed to read length prefix\n";
                break;
            }

            if (msg_len == 0 || msg_len > 65536) {
                std::cerr << "[Native] Rejected invalid message length: " << msg_len << "\n";
                break;
            }

            std::vector<uint8_t> buffer(msg_len);
            r = recv(client_fd, buffer.data(), msg_len, MSG_WAITALL);
            if (r != static_cast<ssize_t>(msg_len)) {
                std::cerr << "[Native] Incomplete message read\n";
                break;
            }

            // Handle the message
            if (port == message_port) {
                queue_message(buffer.data(), msg_len);
            } else if (port == response_port) {
                if (msg_len < 4) {
                    std::cerr << "[Native] Response too short for request_id\n";
                    break;
                }

                uint32_t request_id =
                      (uint32_t(buffer[0])      )
                    | (uint32_t(buffer[1]) << 8 )
                    | (uint32_t(buffer[2]) << 16)
                    | (uint32_t(buffer[3]) << 24);

                deliver_response(request_id, buffer.data() + 4, msg_len - 4);
            } else {
                std::cerr << "[Native] Unknown port: " << port << "\n";
            }
        }

        close(client_fd);
    }

    close(server_fd);
}



void socket_response_listener(int port, in_addr_t ip) {
    if (ip == INADDR_NONE) {
        ip = resolve_ip_or_throw(get_client_ip());  // or get_client_ip()
    }

    socket_listener(port, ip);
}

bool try_connect_with_retry(int sock,
                            const sockaddr_in* addr,
                            socklen_t addrlen,
                            const std::string& tag) {
    int attempt = 0;
    int backoff = INITIAL_BACKOFF_MS;
    while (attempt < MAX_CONNECT_ATTEMPTS) {
        int result = connect(sock, (const sockaddr*)addr, addrlen);
        if (result == 0) {
            return true;  // connected!
        }
        ++attempt;
        std::cerr << tag
                  << " connect attempt " << attempt
                  << " failed (errno=" << errno << "), retrying in "
                  << backoff << "ms...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
        backoff *= 2;
    }
    return false;  // all attempts failed
}

void send_over_socket(const uint8_t* data, uint32_t length, const char* ip, uint16_t port) {
    std::string tag = (port == message_port) ? "[Client] " : "[Server] ";

    std::lock_guard<std::mutex> lock(sock_mutex);

    int& sock = persistent_sockets[port];
    if (sock <= 0) {
        std::cout << "making a socket \n";
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror((tag + " socket").c_str());
            return;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        try {
            server_addr.sin_addr.s_addr = resolve_ip_or_throw(ip);
        } catch (const std::exception& e) {
            std::cerr << "[Native] Failed to resolve '" << ip << "': " << e.what() << "\n";
            close(sock);
            sock = -1;
            return;
        }

        if (!try_connect_with_retry(sock, &server_addr, sizeof(server_addr), tag)) {
            std::fprintf(stderr, "%s Failed to connect to %s:%u\n", tag.c_str(), ip, port);
            close(sock);
            sock = -1;
            return;
        }
    }

    uint32_t len_le = length;
    std::vector<uint8_t> framed(sizeof(len_le) + length);
    std::memcpy(framed.data(), &len_le, sizeof(len_le));
    std::memcpy(framed.data() + sizeof(len_le), data, length);

    ssize_t sent = send(sock, framed.data(), framed.size(), 0);

    if (sent < 0) {
        perror((tag + " send").c_str());
    } else if (static_cast<uint32_t>(sent) != framed.size()) {
        std::fprintf(stderr, "%s Partial send: %zd of %zu bytes\n", tag.c_str(), sent, framed.size());
    }
}


void send_response_over_socket(const uint8_t* data, uint32_t length, const char* ip, uint16_t port) {
    // std::cout << "[Native] Sending response to " << ip << ":" << port << "\n";
    send_over_socket(data, length, ip, port);
}

void send_over_socket(const uint8_t* data, uint32_t length) {
    send_over_socket(data, length, get_server_ip(), message_port);
}

// void send_response_over_socket(const uint8_t* data, uint32_t length, uint32_t request_id) {
//     send_response_over_socket(data, length, get_client_ip(), response_port);
// }

void send_response_over_socket(const uint8_t* data, uint32_t length, uint32_t request_id) {
    // Allocate a temporary buffer for [request_id | data]
    uint32_t total_length = sizeof(uint32_t) + length;
    std::vector<uint8_t> buffer(total_length);

    // Copy request_id to the front (little-endian)
    buffer[0] = uint8_t((request_id >>  0) & 0xFF);
    buffer[1] = uint8_t((request_id >>  8) & 0xFF);
    buffer[2] = uint8_t((request_id >> 16) & 0xFF);
    buffer[3] = uint8_t((request_id >> 24) & 0xFF);

    // Copy original message after the request_id
    std::memcpy(buffer.data() + sizeof(uint32_t), data, length);

    // Call the actual socket send function
    send_response_over_socket(buffer.data(), total_length, get_client_ip(), response_port);
}


in_addr_t resolve_ip_or_throw(const char* hostname) {
    in_addr addr_parsed{};
    if (inet_pton(AF_INET, hostname, &addr_parsed) == 1) {
        return addr_parsed.s_addr;
    }

    hostent* host = gethostbyname(hostname);
    if (!host) {
        throw std::runtime_error(std::string("Failed to resolve hostname: ") + hostname);
    }

    in_addr_t ip;
    std::memcpy(&ip, host->h_addr, host->h_length);
    return ip;
}

void cleanup_sockets() {
    std::lock_guard<std::mutex> lock(sock_mutex);
    for (auto& [port, sock] : persistent_sockets) {
        if (sock >= 0) close(sock);
        sock = -1;
    }
}


