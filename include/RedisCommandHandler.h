#ifndef REDIS_COMMAND_HANDLER_H
#define REDIS_COMMAND_HANDLER_H

#include <chrono>
#include <deque>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

class rediscommandhandler {
public:
  rediscommandhandler();
  ~rediscommandhandler();

  std::string processcommand(const std::string &commandline);

private:
  // Tier 1 & 3: Strings & Expiration
  std::unordered_map<std::string, std::string> database;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> expires;

  // Tier 4: Data Structures
  std::unordered_map<std::string, std::deque<std::string>> lists;
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> hashes;
  std::unordered_map<std::string, std::unordered_set<std::string>> sets;

  // Tier 5: Persistence
  std::ofstream aof_file;
  bool replaying_aof = false;

  bool isExpired(const std::string &key);
  void writeToAof(const std::string &commandline);
  void loadAof();
  bool saveRdb(const std::string &filename = "dump.rdb");
  bool loadRdb(const std::string &filename = "dump.rdb");
};

#endif /*REDIS_COMMAND_HANDLER_H*/
