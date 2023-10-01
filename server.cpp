#include <iostream>
#include <vector>
#include <memory>
#include <exception>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <poll.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>


const size_t k_max_msg = 4096;

class Connection {
private:
    int fd;
    uint8_t rbuf[4 + k_max_msg];
    size_t rbuf_size;
    std::string outgoingMessage;

    void handleRead() {
        ssize_t bytes_received = read(fd, rbuf + rbuf_size, sizeof(rbuf) - rbuf_size);
        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                std::cerr << "Read error" << std::endl;
            } else {
                std::cerr << "Connection closed by client" << std::endl;
            }
            close(fd);
            fd = -1;
            return;
        }

        rbuf_size += bytes_received;

        // For simplicity, let's assume that a message ends with a newline character.
        // This loop extracts all complete messages and handles them.
        while (true) {
            auto newlinePos = std::find(rbuf, rbuf + rbuf_size, '\n');
            if (newlinePos != rbuf + rbuf_size) {
                // Message found, handle it.
                // For this example, we simply prepare an echo response.
                outgoingMessage.assign(rbuf, newlinePos + 1);
                
                memmove(rbuf, newlinePos + 1, rbuf_size - (newlinePos + 1 - rbuf));
                rbuf_size -= (newlinePos + 1 - rbuf);
            } else {
                break;  // No complete message in the buffer.
            }
        }
    }

    void handleWrite() {
        if (outgoingMessage.empty()) {
            return;
        }

        ssize_t bytes_sent = write(fd, outgoingMessage.data(), outgoingMessage.size());
        if (bytes_sent <= 0) {
            if (bytes_sent < 0) {
                std::cerr << "Write error" << std::endl;
            } else {
                std::cerr << "Connection closed by client during write" << std::endl;
            }
            close(fd);
            fd = -1;
            return;
        }

        // Remove the sent portion from the outgoingMessage.
        outgoingMessage.erase(0, bytes_sent);
    }

public:
    Connection(int fd) : fd(fd), rbuf_size(0) {}

    int getFD() const {
        return fd;
    }

    void process() {
        handleRead();
        handleWrite();
    }

    ~Connection() {
        close(fd);
    }
};

class Server {
private:
    int listen_fd;
    std::vector<std::unique_ptr<Connection>> connections;

    void acceptConnection() {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
             std::cerr << "Failed to accept new connection: " << std::strerror(errno) << std::endl;
             return;
        }
        connections.emplace_back(std::make_unique<Connection>(client_fd));
    }

public:
    Server(int port) {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == -1) {
            throw std::runtime_error("Failed to create listening socket");
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            close(listen_fd);
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(listen_fd, 10) == -1) {
            close(listen_fd);
            throw std::runtime_error("Failed to start listening");
        }
    }

    void run() {
        while (true) {
            std::vector<pollfd> pollfds;
            pollfds.push_back({listen_fd, POLLIN, 0});
            for (const auto& conn : connections) {
                pollfds.push_back({conn->getFD(), POLLIN, 0});
            }

            if (poll(pollfds.data(), pollfds.size(), -1) == -1) {
                throw std::runtime_error("Poll failed");
            }

            if (pollfds[0].revents & POLLIN) {
                acceptConnection();
            }

            for (size_t i = 1; i < pollfds.size(); ++i) {
                if (pollfds[i].revents & POLLIN) {
                    connections[i-1]->process();
                }
            }
        }
    }

    ~Server() {
        close(listen_fd);
    }
};

int main() {
    try {
        Server server(1234);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
