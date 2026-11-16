# 💡 Real-World Redis Use Cases

Redis is one of the most versatile in-memory databases in modern cloud infrastructure. Here are real-world architectural patterns using the commands supported by this implementation.

---

## 1. Fast Session & Token Storage (Strings & TTL)

Web applications use Redis to store user session tokens with automatic expiration.

```text
# When a user logs in, store their session with a 1-hour expiration (3600 sec)
SET session:token_abc123 "user_id_42" EX 3600

# When receiving a request, verify their token:
GET session:token_abc123
# Returns: "user_id_42"

# Check remaining session life:
TTL session:token_abc123
# Returns: :3540
```

---

## 2. Background Task Queue (Lists)

Message workers and microservices use Redis lists as FIFO (First-In, First-Out) job queues.

```text
# Producer service pushes jobs to the end of the queue:
RPUSH email_queue "send_welcome_email:user_10"
RPUSH email_queue "send_invoice:user_45"

# Worker process pops the oldest job from the front:
LPOP email_queue
# Returns: "send_welcome_email:user_10"
```

---

## 3. User Profile & Entity Caching (Hashes)

Instead of serializing/deserializing entire JSON strings, Hashes allow you to read and update specific fields individually without rewriting the entire record.

```text
# Store user details:
HSET user:42 username "rudra"
HSET user:42 email "rudra@example.com"
HSET user:42 tier "premium"

# Fetch only the email field:
HGET user:42 email
# Returns: "rudra@example.com"

# Fetch the entire user profile:
HGETALL user:42
```

---

## 4. Unique Tags & Followers (Sets)

Sets automatically deduplicate entries, making them ideal for tags, followers, and online visitor tracking.

```text
# Add article tags (duplicates are automatically ignored):
SADD article:101:tags "cpp" "networking" "redis" "cpp"

# Retrieve all tags:
SMEMBERS article:101:tags
# Returns: "cpp", "networking", "redis" (only 3 unique items)
```
