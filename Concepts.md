# 🧠 Redis Concepts & Architecture

This document breaks down the core concepts behind this C++ Redis implementation.

---

## 1. Redis Serialization Protocol (RESP)

Redis communicates over TCP using a text-based, binary-safe protocol called **RESP**.

### Data Types in RESP:

| Symbol | Data Type | Example |
| :---: | :--- | :--- |
| `+` | Simple String | `+OK\r\n` |
| `-` | Error | `-ERR unknown command\r\n` |
| `:` | Integer | `:100\r\n` |
| `$` | Bulk String | `$5\r\nhello\r\n` |
| `*` | Array | `*2\r\n$4\r\nECHO\r\n$5\r\nworld\r\n` |
| `$-1` | Null Bulk String | `$-1\r\n` (Key not found / expired) |

Every command sent by `redis-cli` arrives as an array of bulk strings:
```text
*2\r\n$4\r\nECHO\r\n$5\r\nhello\r\n
```

---

## 2. Concurrency: Event Loop & I/O Multiplexing

Unlike multi-threaded servers that spawn a thread per client (which consumes substantial RAM and causes lock contention), Redis uses a **single-threaded Event Loop**.

### How `select()` Multiplexing Works:
1. Sockets are kept in non-blocking mode.
2. The operating system monitors a bitmask of file descriptors (`fd_set`).
3. When network traffic arrives on any socket, the OS wakes `select()`.
4. The server dispatches the request, executes the in-memory operation, and replies in microseconds.

---

## 3. Data Structures Implementation

* **Strings**: Backed by `std::unordered_map<std::string, std::string>`.
* **Lists**: Backed by `std::deque<std::string>`, providing $O(1)$ push/pop operations at both ends.
* **Hashes**: Backed by a nested hash map `std::unordered_map<std::string, std::unordered_map<std::string, std::string>>`.
* **Sets**: Backed by `std::unordered_set<std::string>`, guaranteeing unique values and $O(1)$ membership lookup.

---

## 4. Key Expiration & Passive Eviction

Redis manages key expiration using **Passive Eviction**:
* When keys are set with `EX <seconds>`, a deadline timestamp is recorded using `std::chrono::steady_clock`.
* Rather than scanning millions of keys every millisecond, Redis checks the key **passively when it is accessed** (`GET`, `EXISTS`, `TTL`).
* If `now >= deadline`, the key is immediately erased from memory and treated as non-existent.

---

## 5. Persistence Models

1. **AOF (Append-Only File)**:
   * Every write operation is written sequentially to `appendonly.aof`.
   * On server restart, the log is replayed from top to bottom, reconstructing the database state.
2. **RDB (Snapshotting)**:
   * Serializes the current in-memory state into a compact point-in-time snapshot file (`dump.rdb`).
