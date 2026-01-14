#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <thread>
#include <mutex>
#include <unordered_map>

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

static void socket_close(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
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


static void trim_inplace(std::string& s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    size_t start = 0;
    while (start < s.size() && is_space((unsigned char)s[start])) start++;
    size_t end = s.size();
    while (end > start && is_space((unsigned char)s[end - 1])) end--;
    s = s.substr(start, end - start);
}

static std::string upper_copy(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static bool parse_int_strict(const std::string& s, int& out) {
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        int v = std::stoi(s, &pos);
        if (pos != s.size()) return false;
        out = v;
        return true;
    }
    catch (...) {
        return false;
    }
}

class ClientState {
    public:
        std::string name = "Player";
        int x = 0;
        int y = 0;
};

static std::mutex g_mutex;
static std::unordered_map<int, ClientState> g_clients;
static int g_nextClientId = 1;

static void client_thread(socket_t clientSock, sockaddr_in clientAddr) {
    int myInternalId = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        myInternalId = g_nextClientId++;
        g_clients[myInternalId] = ClientState{};
    }

    char ip[64]{};
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
    uint16_t port = ntohs(clientAddr.sin_port);

    std::cout << "[Client#" << myInternalId << "] Connected from " << ip << ":" << port << "\n";

    bool shouldQuit = false;

    while (!shouldQuit) {
        std::string request;
        if (!recv_packet(clientSock, request)) {
            std::cout << "[Client#" << myInternalId << "] Disconnected or recv error\n";
            break;
        }

        // Nettoyage
        request.erase(std::remove(request.begin(), request.end(), '\r'), request.end());
        request.erase(std::remove(request.begin(), request.end(), '\n'), request.end());
        trim_inplace(request);

        if (request.empty()) {
            if (!send_packet(clientSock, "ERR BAD_FORMAT")) break;
            continue;
        }

        std::string cmd, rest;
        size_t sp = request.find(' ');
        if (sp == std::string::npos) { cmd = request; rest = ""; }
        else { cmd = request.substr(0, sp); rest = request.substr(sp + 1); trim_inplace(rest); }

        std::string CMD = upper_copy(cmd);

        if (CMD == "PING") {
            if (!send_packet(clientSock, "PONG")) break;
            continue;
        }

        if (CMD == "NAME") {
            if (rest.empty()) {
                if (!send_packet(clientSock, "ERR BAD_FORMAT")) break;
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_clients[myInternalId].name = rest;
            }
            if (!send_packet(clientSock, "WELCOME " + rest)) break;
            continue;
        }

        if (CMD == "POS") {
            int x, y;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                x = g_clients[myInternalId].x;
                y = g_clients[myInternalId].y;
            }
            if (!send_packet(clientSock, "POS " + std::to_string(x) + " " + std::to_string(y))) break;
            continue;
        }

        if (CMD == "MOVE") {
            size_t sp2 = rest.find(' ');
            if (sp2 == std::string::npos) {
                if (!send_packet(clientSock, "ERR BAD_FORMAT")) break;
                continue;
            }
            std::string a = rest.substr(0, sp2);
            std::string b = rest.substr(sp2 + 1);
            trim_inplace(a); trim_inplace(b);

            int dx = 0, dy = 0;
            if (!parse_int_strict(a, dx) || !parse_int_strict(b, dy)) {
                if (!send_packet(clientSock, "ERR BAD_FORMAT")) break;
                continue;
            }

            int x, y;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                auto& st = g_clients[myInternalId];
                st.x += dx;
                st.y += dy;
                x = st.x; y = st.y;
            }

            if (!send_packet(clientSock, "OK POS " + std::to_string(x) + " " + std::to_string(y))) break;
            continue;
        }

        if (CMD == "QUIT") {
            send_packet(clientSock, "BYE");
            shouldQuit = true;
            break;
        }

        if (!send_packet(clientSock, "ERR UNKNOWN_COMMAND")) break;
    }

    socket_close(clientSock);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_clients.erase(myInternalId);
    }

    std::cout << "[Client#" << myInternalId << "] Closed\n";
}

int main() {
    if (!socket_init()) {
        std::cerr << "socket init failed\n";
        return 1;
    }

    const uint16_t port = 27015;

    socket_t listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET_T) {
        std::cerr << "socket() failed\n";
        socket_shutdown();
        return 1;
    }

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

    if (listen(listenSock, 16) != 0) {
        std::cerr << "listen() failed\n";
        socket_close(listenSock);
        socket_shutdown();
        return 1;
    }

    std::cout << "TCP multi-client server listening on port " << port << "...\n";

    while (true) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif

        socket_t clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock == INVALID_SOCKET_T) {
            std::cerr << "accept() failed\n";
            break;
        }

        std::thread(client_thread, clientSock, clientAddr).detach();
    }

    socket_close(listenSock);
    socket_shutdown();
    return 0;
}
