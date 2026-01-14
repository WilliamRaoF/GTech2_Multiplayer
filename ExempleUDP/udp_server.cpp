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

static bool socket_init() {
#ifdef _WIN32
  WSADATA wsa;
  return WSAStartup(MAKEWORD(2,2), &wsa) == 0;
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

int main() {
  if (!socket_init()) {
    std::cerr << "socket init failed\n";
    return 1;
  }

  const uint16_t port = 27016;

  socket_t sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET_T) {
    std::cerr << "socket() failed\n";
    socket_shutdown();
    return 1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
    std::cerr << "bind() failed\n";
    socket_close(sock);
    socket_shutdown();
    return 1;
  }

  std::cout << "UDP server listening on port " << port << "...\n";

  std::vector<char> buffer(2048);

  while (true) {
    sockaddr_in from{};
#ifdef _WIN32
    int fromLen = sizeof(from);
#else
    socklen_t fromLen = sizeof(from);
#endif

    int received = recvfrom(sock, buffer.data(), (int)buffer.size() - 1, 0,
                            (sockaddr*)&from, &fromLen);
    if (received < 0) {
      std::cerr << "recvfrom() error\n";
      break;
    }

    buffer[received] = '\0';

    char ip[64]{};
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
    std::cout << "From " << ip << ":" << ntohs(from.sin_port)
              << " -> " << buffer.data() << "\n";

    // Réponse optionnelle (debug)
    const char* reply = "OK";
    sendto(sock, reply, (int)std::strlen(reply), 0, (sockaddr*)&from, fromLen);
  }

  socket_close(sock);
  socket_shutdown();
  return 0;
}
