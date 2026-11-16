#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close_socket(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define close_socket(s) close(s)
#endif

int main() {
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed\n";
    return 1;
  }
#endif

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    std::cerr << "Error creating socket\n";
    return 1;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(6379);
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "Could not connect to Redis at 127.0.0.1:6379. Is the server running?\n";
    close_socket(sock);
    return 1;
  }

  std::cout << "Connected to Redis at " << sock << "\n";

  std::string input;
  char buffer[2048];

  while (true) {
    std::cout << "127.0.0.1:6379> ";
    if (!std::getline(std::cin, input))
      break;
    if (input.empty())
      continue;
    if (input == "exit" || input == "quit")
      break;

    std::string cmd = input + "\r\n";
    send(sock, cmd.c_str(), cmd.size(), 0);

    memset(buffer, 0, sizeof(buffer));
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
      std::cout << buffer;
    } else {
      std::cout << "Server closed connection.\n";
      break;
    }
  }

  close_socket(sock);
#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}

