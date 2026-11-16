#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include <atomic>
#include <string>

class redisserver {

public:
  redisserver(int port);
  void run();
  void shutdown();

private:
  int port;
  int server_socket;
  std::atomic<bool> running;
};

#endif