    #include <iostream>
    #include <vector>
    #include <string>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/ip.h>
    #include <stdexcept>
    #include <cstring>
    #include <unistd.h>
    #include <cerrno>

    const size_t k_max_msg = 4096;

    class Client {
    private:
        int fd;

        void die(const std::string& msg) const {
    std::cerr << msg << ": " << std::strerror(errno) << std::endl;
    throw std::runtime_error(msg);
}


        int32_t read_full(char *buf, size_t n) const {
            while (n > 0) {
                ssize_t rv = read(fd, buf, n);
                if (rv <= 0) {
                    if (rv == 0 || errno == EINTR) return -1; // EOF or interrupted
                    die("read() error");
                }
                n -= rv;
                buf += rv;
            }
            return 0;
        }

        int32_t write_all(const char *buf, size_t n) const {
            while (n > 0) {
                ssize_t rv = write(fd, buf, n);
                if (rv <= 0) {
                    if (rv == 0 || errno == EINTR) return -1; // EOF or interrupted
                    die("write() error");
                }
                n -= rv;
                buf += rv;
            }
            return 0;
        }

    public:
        Client(const std::string& ip, int port) {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) {
                die("socket()");
            }

            struct sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            if (inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr)) <= 0) {
                die("inet_pton");
            }

            if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr))) {
                die("connect");
            }
        }

        void sendRequest(const std::vector<std::string>& cmd) {
            uint32_t len = 8; // Base length includes total size, number of strings
            for (const auto& s : cmd) {
                len += 4 + s.size(); // Each string has a 4-byte size prefix
            }
            if (len - 4 > k_max_msg) { // `- 4` because the message length does not include itself
                throw std::runtime_error("Message too long");
            }

            std::vector<char> wbuf(len);
            memcpy(&wbuf[0], &len, 4);
            uint32_t n = cmd.size();
            memcpy(&wbuf[4], &n, 4);

            size_t cur = 8;
            for (const auto& s : cmd) {
                uint32_t p = s.size();
                memcpy(&wbuf[cur], &p, 4);
                memcpy(&wbuf[cur + 4], s.data(), s.size());
                cur += 4 + s.size();
            }

            if (write_all(wbuf.data(), wbuf.size())) {
                die("Failed to send the entire request");
            }
        }

        void receiveResponse() {
            char rbuf[4 + k_max_msg + 1];

            if (read_full(rbuf, 4)) {
                throw std::runtime_error("Failed to read response header");
            }

            uint32_t len;
            memcpy(&len, rbuf, 4);
            if (len > k_max_msg) {
                throw std::runtime_error("Response too long");
            }

            if (read_full(&rbuf[4], len)) {
                throw std::runtime_error("Failed to read response body");
            }

            uint32_t rescode;
            memcpy(&rescode, &rbuf[4], 4);
            std::cout << "server says: [" << rescode << "] " << std::string(&rbuf[8], len - 4) << "\n";
        }

        ~Client() {
            close(fd);
        }
    };

    int main(int argc, char **argv) {
        try {
            if (argc < 3) {
                std::cerr << "Usage: client <IP> <PORT> [cmd...]" << std::endl;
                return 1;
            }

            Client client(argv[1], std::stoi(argv[2]));

            std::vector<std::string> cmd;
            for (int i = 3; i < argc; ++i) {
                cmd.push_back(argv[i]);
            }

            client.sendRequest(cmd);
            client.receiveResponse();
        } catch (const std::exception& e) {
            std::cerr << "Client Error: " << e.what() << std::endl;
            return 1;
        }

        return 0;
    }
