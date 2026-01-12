// tcp_server.cpp
// Serveur TCP: accepte 1 client, lit des lignes, renvoie "echo: <ligne>"

#include <iostream>
#include <string>
#include <vector>
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

    const uint16_t port = 27015;

    socket_t listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET_T) {
        std::cerr << "socket() failed\n";
        socket_shutdown();
        return 1;
    }

    // Permet de relancer rapidement le serveur (Linux/macOS)
#ifndef _WIN32
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "bind() failed\n";
        socket_close(listenSock);
        socket_shutdown();
        return 1;
    }

    if (listen(listenSock, 1) != 0) {
        std::cerr << "listen() failed\n";
        socket_close(listenSock);
        socket_shutdown();
        return 1;
    }

    std::cout << "TCP server listening on port " << port << "...\n";

    sockaddr_in clientAddr{};
#ifdef _WIN32
    int clientLen = sizeof(clientAddr);
#else
    socklen_t clientLen = sizeof(clientAddr);
#endif

    socket_t clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
    if (clientSock == INVALID_SOCKET_T) {
        std::cerr << "accept() failed\n";
        socket_close(listenSock);
        socket_shutdown();
        return 1;
    }

    char clientIp[64]{};
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    std::cout << "Client connected from " << clientIp << ":" << ntohs(clientAddr.sin_port) << "\n";

    // Boucle lecture -> renvoi (echo)
    std::vector<char> buffer(1024);

    while (true) {
        int received = recv(clientSock, buffer.data(), (int)buffer.size(), 0);
        if (received == 0) {
            std::cout << "Client disconnected.\n";
            break;
        }
        if (received < 0) {
            std::cerr << "recv() error\n";
            break;
        }

        std::string msg(buffer.data(), buffer.data() + received);
        std::cout << "Received: " << msg;

        std::string reply = "echo: " + msg;
        int sent = send(clientSock, reply.c_str(), (int)reply.size(), 0);
        if (sent < 0) {
            std::cerr << "send() error\n";
            break;
        }
    }

    socket_close(clientSock);
    socket_close(listenSock);
    socket_shutdown();
    return 0;
}
