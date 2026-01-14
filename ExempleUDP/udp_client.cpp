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

  const char* serverIp = "127.0.0.1";
  const uint16_t serverPort = 27016;

  socket_t sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET_T) {
    std::cerr << "socket() failed\n";
    socket_shutdown();
    return 1;
  }

  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(serverPort);
  inet_pton(AF_INET, serverIp, &server.sin_addr);

  std::cout << "UDP client sending to " << serverIp << ":" << serverPort << "\n";
  std::cout << "Type a message and press Enter (type 'quit' to exit)\n";

  std::string line;
  std::vector<char> buffer(2048);

  while (std::getline(std::cin, line)) {
    if (line == "quit") break;

    int sent = sendto(sock, line.c_str(), (int)line.size(), 0,
                      (sockaddr*)&server, sizeof(server));
    if (sent < 0) {
      std::cerr << "sendto() error\n";
      break;
    }

    // Attendre une réponse "OK" (optionnel)
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
    std::cout << "Server reply: " << buffer.data() << "\n";
  }

  socket_close(sock);
  socket_shutdown();
  return 0;
}
