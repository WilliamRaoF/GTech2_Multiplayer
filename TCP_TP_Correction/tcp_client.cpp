#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
const socket_t INVALID_SOCKET_T = INVALID_SOCKET;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
const socket_t INVALID_SOCKET_T = -1;
#endif

static void socket_close(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static bool socket_init() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

static void socket_shutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

static bool send_all(socket_t sock, const void* data, size_t len) {
    const char* p = (const char*)data;
    while (len > 0) {
        int sent = send(sock, p, (int)len, 0);
        if (sent <= 0) return false;
        p += sent;
        len -= (size_t)sent;
    }
    return true;
}

static bool recv_exact(socket_t sock, void* data, size_t len) {
    char* p = (char*)data;
    while (len > 0) {
        int r = recv(sock, p, (int)len, 0);
        if (r == 0) return false;
        if (r < 0) return false;
        p += r;
        len -= (size_t)r;
    }
    return true;
}

static bool send_packet(socket_t sock, const std::string& msg) {
    uint32_t n = (uint32_t)msg.size();
    uint32_t net_n = htonl(n);
    if (!send_all(sock, &net_n, sizeof(net_n))) return false;
    if (n > 0 && !send_all(sock, msg.data(), n)) return false;
    return true;
}

static bool recv_packet(socket_t sock, std::string& out) {
    uint32_t net_n = 0;
    if (!recv_exact(sock, &net_n, sizeof(net_n))) return false;
    uint32_t n = ntohl(net_n);

    const uint32_t MAX = 4096;
    if (n > MAX) return false;

    out.clear();
    out.resize(n);

    if (n > 0) {
        if (!recv_exact(sock, &out[0], n)) return false;
    }
    return true;
}


int main() {
    if (!socket_init()) {
        std::cerr << "socket init failed\n";
        return 1;
    }

    const char* serverIp = "127.0.0.1";
    const uint16_t port = 27015;

    socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_T) {
        std::cerr << "socket() failed\n";
        socket_shutdown();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, serverIp, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "connect() failed\n";
        socket_close(sock);
        socket_shutdown();
        return 1;
    }

    std::cout << "Connected to " << serverIp << ":" << port << "\n";
    std::cout << "Commands: PING | NAME <pseudo> | POS | MOVE dx dy | QUIT\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;

        if (!send_packet(sock, line)) {
            std::cerr << "send_packet failed\n";
            break;
        }

        std::string response;
        if (!recv_packet(sock, response)) {
            std::cerr << "recv_packet failed or server closed\n";
            break;
        }

        std::cout << response << "\n";

        if (response == "BYE") break;
    }

    socket_close(sock);
    socket_shutdown();
    return 0;
}
