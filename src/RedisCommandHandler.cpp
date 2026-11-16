#include "../include/RedisCommandHandler.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// RESP parser:
// *2\r\n$4\r\nPING\r\n$4\r\nTEST\r\n
// *2 -> array has 2 elements
// $4 -> next string has 4 characters
// PING
// TEST

std::vector<std::string> parseRespCommand(const std::string &input) {
  std::vector<std::string> tokens;
  if (input.empty())
    return tokens;

  if (input[0] != '*') {
    std::istringstream iss(input);
    std::string token;

    while (iss >> token) {
      tokens.push_back(token);
    }
    return tokens;
  }
  size_t pos = 0;
  if (input[pos] != '*')
    return tokens;
  pos++;

  size_t crlf = input.find("\r\n", pos);
  if (crlf == std::string::npos)
    return tokens;

  int numelements = std::stoi(input.substr(pos, crlf - pos));
  pos = crlf + 2;

  for (int i = 0; i < numelements; i++) {

    if (pos >= input.size() || input[pos] != '$')
      break;
    pos++;
    crlf = input.find("\r\n", pos);
    if (crlf == std::string::npos)
      break;

    int len = std::stoi(input.substr(pos, crlf - pos));
    pos = crlf + 2;
    if (pos + len > input.size())
      break;
    std::string token = input.substr(pos, len);
    tokens.push_back(token);
    pos += len + 2;
  }
  return tokens;
}
rediscommandhandler::rediscommandhandler() {
  loadAof();
  aof_file.open("appendonly.aof", std::ios::app);
}

rediscommandhandler::~rediscommandhandler() {
  if (aof_file.is_open()) {
    aof_file.close();
  }
}

void rediscommandhandler::writeToAof(const std::string &commandline) {
  if (!replaying_aof && aof_file.is_open()) {
    aof_file << commandline;
    if (commandline.empty() || commandline.back() != '\n') {
      aof_file << "\n";
    }
    aof_file.flush();
  }
}

void rediscommandhandler::loadAof() {
  std::ifstream infile("appendonly.aof");
  if (!infile.is_open()) {
    return;
  }
  replaying_aof = true;
  std::string line;
  while (std::getline(infile, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      processcommand(line);
    }
  }
  replaying_aof = false;
  infile.close();
}

bool rediscommandhandler::saveRdb(const std::string &filename) {
  std::ofstream out(filename);
  if (!out.is_open())
    return false;

  out << "# Redis Snapshot Dump\n";
  for (const auto &pair : database) {
    if (!isExpired(pair.first)) {
      out << "SET " << pair.first << " " << pair.second << "\n";
    }
  }
  for (const auto &pair : lists) {
    for (const auto &item : pair.second) {
      out << "RPUSH " << pair.first << " " << item << "\n";
    }
  }
  for (const auto &pair : hashes) {
    for (const auto &field : pair.second) {
      out << "HSET " << pair.first << " " << field.first << " " << field.second
          << "\n";
    }
  }
  for (const auto &pair : sets) {
    for (const auto &member : pair.second) {
      out << "SADD " << pair.first << " " << member << "\n";
    }
  }
  out.close();
  return true;
}

bool rediscommandhandler::isExpired(const std::string &key) {
  auto it = expires.find(key);
  if (it == expires.end()) {
    return false; // No expiration set on this key
  }
  if (std::chrono::steady_clock::now() >= it->second) {
    // Passive deletion: the time has passed!
    database.erase(key);
    expires.erase(it);
    return true;
  }
  return false;
}

std::string rediscommandhandler::processcommand(const std::string &commandline) {
  auto tokens = parseRespCommand(commandline);

  if (tokens.empty())
    return "error: empty command\r\n";
  for (auto &t : tokens) {
    std::cout << t << "\n";
  }

  std::string cmd = tokens[0];
  std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

  if (cmd == "PING") {
    return "+PONG\r\n";
  } else if (cmd == "ECHO") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'echo' command\r\n";
    return "+" + tokens[1] + "\r\n";
  } else if (cmd == "SET") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'set' command\r\n";

    database[tokens[1]] = tokens[2];
    expires.erase(tokens[1]); // Reset any previous expiration

    // Check for optional: SET key value EX seconds
    if (tokens.size() >= 5) {
      std::string opt = tokens[3];
      std::transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
      if (opt == "EX") {
        try {
          int seconds = std::stoi(tokens[4]);
          expires[tokens[1]] = std::chrono::steady_clock::now() +
                               std::chrono::seconds(seconds);
        } catch (...) {
          return "-ERR value is not an integer or out of range\r\n";
        }
      }
    }
    writeToAof(commandline);
    return "+OK\r\n";
  } else if (cmd == "GET") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'get' command\r\n";

    if (isExpired(tokens[1])) {
      return "$-1\r\n"; // Expired key is treated as deleted
    }

    auto it = database.find(tokens[1]);
    if (it != database.end()) {
      return "$" + std::to_string(it->second.size()) + "\r\n" + it->second +
             "\r\n";
    } else {
      return "$-1\r\n"; // Redis null bulk string (key not found)
    }
  } else if (cmd == "EXPIRE") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'expire' command\r\n";

    if (isExpired(tokens[1]) || database.find(tokens[1]) == database.end()) {
      return ":0\r\n"; // Key does not exist
    }

    try {
      int seconds = std::stoi(tokens[2]);
      expires[tokens[1]] = std::chrono::steady_clock::now() +
                           std::chrono::seconds(seconds);
      writeToAof(commandline);
      return ":1\r\n"; // Timeout was set
    } catch (...) {
      return "-ERR value is not an integer or out of range\r\n";
    }
  } else if (cmd == "TTL") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'ttl' command\r\n";

    if (isExpired(tokens[1]) || database.find(tokens[1]) == database.end()) {
      return ":-2\r\n"; // Redis standard: key does not exist
    }

    auto it = expires.find(tokens[1]);
    if (it == expires.end()) {
      return ":-1\r\n"; // Redis standard: key exists but has no expiration
    }

    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                         it->second - std::chrono::steady_clock::now())
                         .count();
    if (remaining < 0)
      remaining = 0;
    return ":" + std::to_string(remaining) + "\r\n";
  } else if (cmd == "DEL") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'del' command\r\n";

    isExpired(tokens[1]);
    expires.erase(tokens[1]);
    size_t count = database.erase(tokens[1]);
    if (count > 0)
      writeToAof(commandline);
    return ":" + std::to_string(count) + "\r\n"; // Redis integer reply
  } else if (cmd == "EXISTS") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'exists' command\r\n";

    if (isExpired(tokens[1])) {
      return ":0\r\n";
    }
    int count = (database.find(tokens[1]) != database.end()) ? 1 : 0;
    return ":" + std::to_string(count) + "\r\n";
  }
  // --- TIER 4: LISTS (LPUSH, RPUSH, LPOP, RPOP, LRANGE) ---
  else if (cmd == "LPUSH") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'lpush' command\r\n";
    auto &lst = lists[tokens[1]];
    for (size_t i = 2; i < tokens.size(); i++) {
      lst.push_front(tokens[i]);
    }
    writeToAof(commandline);
    return ":" + std::to_string(lst.size()) + "\r\n";
  } else if (cmd == "RPUSH") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'rpush' command\r\n";
    auto &lst = lists[tokens[1]];
    for (size_t i = 2; i < tokens.size(); i++) {
      lst.push_back(tokens[i]);
    }
    writeToAof(commandline);
    return ":" + std::to_string(lst.size()) + "\r\n";
  } else if (cmd == "LPOP") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'lpop' command\r\n";
    auto it = lists.find(tokens[1]);
    if (it == lists.end() || it->second.empty())
      return "$-1\r\n";
    std::string val = it->second.front();
    it->second.pop_front();
    if (it->second.empty())
      lists.erase(it);
    writeToAof(commandline);
    return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
  } else if (cmd == "RPOP") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'rpop' command\r\n";
    auto it = lists.find(tokens[1]);
    if (it == lists.end() || it->second.empty())
      return "$-1\r\n";
    std::string val = it->second.back();
    it->second.pop_back();
    if (it->second.empty())
      lists.erase(it);
    writeToAof(commandline);
    return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
  } else if (cmd == "LRANGE") {
    if (tokens.size() < 4)
      return "-ERR wrong number of arguments for 'lrange' command\r\n";
    auto it = lists.find(tokens[1]);
    if (it == lists.end() || it->second.empty())
      return "*0\r\n";

    int start = std::stoi(tokens[2]);
    int stop = std::stoi(tokens[3]);
    int n = static_cast<int>(it->second.size());
    if (start < 0)
      start = n + start;
    if (stop < 0)
      stop = n + stop;
    if (start < 0)
      start = 0;
    if (stop >= n)
      stop = n - 1;
    if (start > stop || start >= n)
      return "*0\r\n";

    int count = stop - start + 1;
    std::string res = "*" + std::to_string(count) + "\r\n";
    for (int i = start; i <= stop; i++) {
      res += "$" + std::to_string(it->second[i].size()) + "\r\n" +
             it->second[i] + "\r\n";
    }
    return res;
  }
  // --- TIER 4: HASHES (HSET, HGET, HGETALL, HDEL) ---
  else if (cmd == "HSET") {
    if (tokens.size() < 4)
      return "-ERR wrong number of arguments for 'hset' command\r\n";
    bool isNew =
        (hashes[tokens[1]].find(tokens[2]) == hashes[tokens[1]].end());
    hashes[tokens[1]][tokens[2]] = tokens[3];
    writeToAof(commandline);
    return ":" + std::string(isNew ? "1" : "0") + "\r\n";
  } else if (cmd == "HGET") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'hget' command\r\n";
    auto it = hashes.find(tokens[1]);
    if (it == hashes.end())
      return "$-1\r\n";
    auto fIt = it->second.find(tokens[2]);
    if (fIt == it->second.end())
      return "$-1\r\n";
    return "$" + std::to_string(fIt->second.size()) + "\r\n" + fIt->second +
           "\r\n";
  } else if (cmd == "HGETALL") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'hgetall' command\r\n";
    auto it = hashes.find(tokens[1]);
    if (it == hashes.end() || it->second.empty())
      return "*0\r\n";
    std::string res = "*" + std::to_string(it->second.size() * 2) + "\r\n";
    for (const auto &pair : it->second) {
      res += "$" + std::to_string(pair.first.size()) + "\r\n" + pair.first +
             "\r\n";
      res += "$" + std::to_string(pair.second.size()) + "\r\n" + pair.second +
             "\r\n";
    }
    return res;
  } else if (cmd == "HDEL") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'hdel' command\r\n";
    auto it = hashes.find(tokens[1]);
    if (it == hashes.end())
      return ":0\r\n";
    size_t count = it->second.erase(tokens[2]);
    if (it->second.empty())
      hashes.erase(it);
    if (count > 0)
      writeToAof(commandline);
    return ":" + std::to_string(count) + "\r\n";
  }
  // --- TIER 4: SETS (SADD, SMEMBERS, SREM) ---
  else if (cmd == "SADD") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'sadd' command\r\n";
    auto &s = sets[tokens[1]];
    int added = 0;
    for (size_t i = 2; i < tokens.size(); i++) {
      if (s.insert(tokens[i]).second) {
        added++;
      }
    }
    if (added > 0)
      writeToAof(commandline);
    return ":" + std::to_string(added) + "\r\n";
  } else if (cmd == "SMEMBERS") {
    if (tokens.size() < 2)
      return "-ERR wrong number of arguments for 'smembers' command\r\n";
    auto it = sets.find(tokens[1]);
    if (it == sets.end() || it->second.empty())
      return "*0\r\n";
    std::string res = "*" + std::to_string(it->second.size()) + "\r\n";
    for (const auto &member : it->second) {
      res += "$" + std::to_string(member.size()) + "\r\n" + member + "\r\n";
    }
    return res;
  } else if (cmd == "SREM") {
    if (tokens.size() < 3)
      return "-ERR wrong number of arguments for 'srem' command\r\n";
    auto it = sets.find(tokens[1]);
    if (it == sets.end())
      return ":0\r\n";
    size_t count = it->second.erase(tokens[2]);
    if (it->second.empty())
      sets.erase(it);
    if (count > 0)
      writeToAof(commandline);
    return ":" + std::to_string(count) + "\r\n";
  }
  // --- TIER 5: PERSISTENCE (SAVE / RDB SNAPSHOT) ---
  else if (cmd == "SAVE") {
    if (saveRdb("dump.rdb")) {
      return "+OK\r\n";
    } else {
      return "-ERR failed to save dump.rdb\r\n";
    }
  } else {
    return "-ERR unknown command '" + cmd + "'\r\n";
  }
}