# ⚡ Redis In C++

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-green.svg)]()
[![Protocol](https://img.shields.io/badge/Protocol-RESP-red.svg)](https://redis.io/docs/reference/protocol-spec/)

A lightweight, high-performance in-memory key-value database and caching engine built from scratch in modern **C++17**, fully adhering to the official **Redis Serialization Protocol (RESP)**.

Includes an **I/O Multiplexed Event Loop** for concurrent multi-client connectivity, multiple core Redis data structures (Strings, Lists, Hashes, Sets), time-based key expiration (TTL), and full durability via Append-Only File (AOF) persistence and snapshotting.

---

## 🚀 Key Features

* **⚡ Concurrency with I/O Multiplexing**: Built on non-blocking `select()` sockets, allowing dozens of clients to communicate simultaneously over TCP without thread overhead or race conditions.
* **📜 Official RESP Protocol**: Accurately parses and serializes Redis arrays, bulk strings, integers, simple strings, and error replies.
* **📦 Rich Data Structures**:
  * **Strings**: Fast $O(1)$ key-value retrieval and updates.
  * **Lists**: Double-ended queues (`std::deque`) supporting push/pop from both head and tail, plus range slicing.
  * **Hashes**: Field-value mapping objects (`std::unordered_map`) for structured document storage.
  * **Sets**: Deduplicated string collections (`std::unordered_set`) with member validation.
* **⏳ Time-To-Live (TTL) & Expiration**: Passive deletion engine that expires keys on access, supporting `EXPIRE`, `TTL`, and `SET ... EX`.
* **💾 Data Persistence (AOF & RDB)**:
  * **AOF (Append-Only File)**: Logs every state-mutating command to `appendonly.aof` and automatically replays it on startup to restore memory state.
  * **RDB (Snapshotting)**: On-demand memory dump to `dump.rdb` using `SAVE`.
* **🖥️ Dedicated Interactive CLI**: Includes a companion terminal client (`my_redis_cli`) for seamless manual testing.
* **🌐 Cross-Platform**: Works natively on **Windows** (using Winsock2) and **Linux/macOS** (using POSIX sockets).

---

## 🏗️ Architecture

```
                               ┌─────────────────────────┐
                               │  Redis Client (CLI/App) │
                               └────────────┬────────────┘
                                            │ TCP Socket (Port 6379)
                                            ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                 Redis Server (C++)                                     │
│                                                                                        │
│   ┌────────────────────────────────────────────────────────────────────────────────┐   │
│   │               I/O Multiplexing Event Loop (select() / Non-Blocking)            │   │
│   └───────────────────────────────────────┬────────────────────────────────────────┘   │
│                                           │ Dispatches Commands                        │
│                                           ▼                                            │
│   ┌────────────────────────────────────────────────────────────────────────────────┐   │
│   │                      RESP Parser & Command Dispatcher                          │   │
│   └───────────────────────────────────────┬────────────────────────────────────────┘   │
│                                           │ Updates / Reads                            │
│                                           ▼                                            │
│   ┌────────────────────────────────────────────────────────────────────────────────┐   │
│   │                           In-Memory Data Engine                                │   │
│   │  • Strings (unordered_map)              • Lists (deque)                       │   │
│   │  • Hashes (nested unordered_map)        • Sets (unordered_set)                 │   │
│   │  • Expiration Registry (std::chrono)    • AOF Logger & Snapshot Engine         │   │
│   └────────────────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🛠️ Supported Commands

| Category | Command | Syntax | Description |
| :--- | :--- | :--- | :--- |
| **System** | `PING` | `PING` | Tests server health; returns `+PONG`. |
| | `ECHO` | `ECHO <message>` | Echoes back the supplied message. |
| **Strings** | `SET` | `SET <key> <val> [EX <sec>]` | Stores a string, optionally setting TTL. |
| | `GET` | `GET <key>` | Retrieves the value or returns `$-1` if missing/expired. |
| | `DEL` | `DEL <key>` | Deletes the specified key; returns count removed. |
| | `EXISTS` | `EXISTS <key>` | Returns `1` if key exists, `0` otherwise. |
| **Expiration** | `EXPIRE` | `EXPIRE <key> <seconds>` | Sets time-to-live in seconds on a key. |
| | `TTL` | `TTL <key>` | Returns remaining seconds, `-1` (no TTL), or `-2` (missing). |
| **Lists** | `LPUSH` | `LPUSH <key> <val...>` | Inserts element(s) at head of list. |
| | `RPUSH` | `RPUSH <key> <val...>` | Inserts element(s) at tail of list. |
| | `LPOP` | `LPOP <key>` | Removes and returns the head element. |
| | `RPOP` | `RPOP <key>` | Removes and returns the tail element. |
| | `LRANGE`| `LRANGE <key> <start> <stop>` | Returns range of elements (supports negative indices). |
| **Hashes** | `HSET` | `HSET <key> <field> <val>` | Sets field in hash; returns `1` if new, `0` if updated. |
| | `HGET` | `HGET <key> <field>` | Retrieves value of a specific field in hash. |
| | `HGETALL`| `HGETALL <key>` | Returns all fields and values in the hash. |
| | `HDEL` | `HDEL <key> <field>` | Removes field from hash. |
| **Sets** | `SADD` | `SADD <key> <member...>` | Adds unique member(s) to set. |
| | `SMEMBERS`| `SMEMBERS <key>` | Lists all members of the set. |
| | `SREM` | `SREM <key> <member>` | Removes member from the set. |
| **Persistence**| `SAVE` | `SAVE` | Flushes all memory structures to `dump.rdb`. |

---

## 📁 Project Directory Layout

```text
.
├── include/
│   ├── RedisCommandHandler.h  # Protocol parser, storage engine, data structures & AOF
│   └── RedisServer.h          # Networking interface & lifecycle definitions
├── Redis-Client/
│   └── cli.cpp                # Interactive REPL CLI client
├── src/
│   ├── main.cpp               # Server entry point & CLI port parsing
│   ├── RedisCommandHandler.cpp# Execution logic for all commands & serialization
│   └── RedisServer.cpp        # TCP socket engine & I/O event multiplexing
├── Concepts.md                # Architecture notes & reference documentation
├── UseCases.md               # Real-world usage examples & caching patterns
├── Makefile                   # Multi-target build script
└── README.md                  # Project documentation
```

---

## 💻 Building and Running

### Prerequisites
* Any C++17 compatible compiler:
  * **Windows**: MinGW-w64 or MSVC
  * **Linux / WSL / macOS**: GCC (`g++`) or Clang

### 1. Build Both Server & Client
Using GNU Make:
```bash
# On Windows PowerShell (MinGW):
mingw32-make

# On Linux / macOS / WSL:
make
```

This compiles both targets into:
* `my_redis_server.exe` (Server engine)
* `my_redis_cli.exe` (Interactive client)

---

### 2. Start the Server
```bash
# Run on default port 6379:
./my_redis_server

# Or specify a custom port:
./my_redis_server 8080
```

---

### 3. Connect via the Client
In a separate terminal:
```bash
./my_redis_cli
```

---

## 🧪 Interactive Walkthrough

```text
Connected to Redis at socket 3
127.0.0.1:6379> PING
+PONG

127.0.0.1:6379> SET user "Rudra Nagar" EX 15
+OK

127.0.0.1:6379> TTL user
:12

127.0.0.1:6379> GET user
$11
Rudra Nagar

127.0.0.1:6379> RPUSH tasks "code" "test" "deploy"
:3

127.0.0.1:6379> LRANGE tasks 0 -1
*3
$4
code
$4
test
$6
deploy

127.0.0.1:6379> HSET user:1 name "Rudra"
:1
127.0.0.1:6379> HSET user:1 role "Admin"
:1
127.0.0.1:6379> HGETALL user:1
*4
$4
name
$5
Rudra
$4
role
$5
Admin

127.0.0.1:6379> SAVE
+OK
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
