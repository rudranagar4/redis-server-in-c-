#include "../include/RedisServer.h"
#include "../include/RedisCommandHandler.h"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>

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

static redisserver *globalserver = nullptr;

redisserver::redisserver(int port)
    : port(port), server_socket(-1), running(true) {
  globalserver = this;
}

void redisserver::shutdown() {
  running = false;
  if (server_socket != -1) {
    close_socket(server_socket);
    server_socket = -1;
  }
#ifdef _WIN32
  WSACleanup();
#endif
  std::cout << "server shutdown complete!\n";
}

void redisserver::run() {
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed\n";
    return;
  }
#endif

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    std::cerr << "error creating server socket\n";
    return;
  }
  int opt = 1;
#ifdef _WIN32
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
             sizeof(opt));
#else
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(server_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
      0) {
    std::cerr << "Error Binding Server Socket\n";
    return;
  }

  if (listen(server_socket, 10) < 0) {
    std::cerr << "Error Listening On Server Socket\n";
    return;
  }
  std::cout << "Redis Server Listening On Port " << port << "\n";
  rediscommandhandler cmdHandler;

  std::vector<int> client_sockets;

  while (running) {
    fd_set read_fds;
    FD_ZERO(&read_fds);

    // 1. Add listening server socket to monitor new incoming connections
    FD_SET(server_socket, &read_fds);
    int max_fd = server_socket;

    // 2. Add all currently connected client sockets to monitor incoming commands
    for (int client : client_sockets) {
      FD_SET(client, &read_fds);
      if (client > max_fd) {
        max_fd = client;
      }
    }

    // Timeout (100ms) so the loop stays responsive to shutdown signals
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (activity < 0) {
      if (!running)
        break;
      continue;
    }

    // Check if a new client is connecting
    if (FD_ISSET(server_socket, &read_fds)) {
      int client_socket = accept(server_socket, nullptr, nullptr);
      if (client_socket >= 0) {
        client_sockets.push_back(client_socket);
        std::cout << "Client connected (socket " << client_socket << ")!\n";
      }
    }

    // Check if any existing client sent a message
    for (auto it = client_sockets.begin(); it != client_sockets.end();) {
      int client = *it;
      if (FD_ISSET(client, &read_fds)) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
          // Client closed connection or error
          std::cout << "Client disconnected (socket " << client << ")\n";
          close_socket(client);
          it = client_sockets.erase(it);
          continue;
        } else {
          std::string request(buffer, bytes);
          std::string response = cmdHandler.processcommand(request);
          send(client, response.c_str(), response.size(), 0);
        }
      }
      ++it;
    }
  }

  for (int client : client_sockets) {
    close_socket(client);
  }
}