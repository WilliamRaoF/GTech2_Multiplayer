// tcp_client.cpp
// Client TCP: se connecte, envoie des lignes tapées, affiche la réponse.

#include <iostream>
#include <string>
#include <cstring>

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

int main() {
    if (!socket_init()) {
        std::cerr << "WSAStartup failed\n";
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

    std::cout << "Connected to TCP server " << serverIp << ":" << port << "\n";
    std::cout << "Type a line and press Enter (type 'quit' to exit)\n";

    std::string line;
    char buf[2048];

    while (std::getline(std::cin, line)) {
        line += "\n";
        if (line == "quit\n") break;

        int sent = send(sock, line.c_str(), (int)line.size(), 0);
        if (sent < 0) {
            std::cerr << "send() error\n";
            break;
        }

        int received = recv(sock, buf, (int)sizeof(buf), 0);
        if (received <= 0) {
            std::cerr << "recv() failed or server closed\n";
            break;
        }

        std::cout << "Server says: " << std::string(buf, buf + received);
    }

    socket_close(sock);
    socket_shutdown();
    return 0;
}
