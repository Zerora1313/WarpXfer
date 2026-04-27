# 🚀 WarpXfer - High-Speed P2P File Transfer Utility

WarpXfer is a high-performance peer-to-peer file transfer system built using raw TCP sockets in C++. It enables fast, reliable, and concurrent file transfers across local networks.

---

## ✨ Features
- ⚡ High-speed file transfer (100+ Mbps)
- 🔄 Asynchronous event-driven architecture using epoll
- 👥 Handles multiple clients concurrently
- 🔐 Data integrity using SHA-256 chunk validation
- 📦 Low memory overhead with chunk-based transmission

---

## 🛠 Tech Stack
- C++
- TCP Sockets
- POSIX APIs
- epoll (Linux)

---

## 🧠 Key Concepts
- Socket Programming
- Event-driven Architecture
- Concurrency Handling
- File Chunking & Hash Validation

---

## ⚙️ How It Works
1. Client connects to peer using TCP
2. File is divided into chunks
3. Each chunk is transmitted with hash validation
4. Receiver reconstructs file ensuring integrity

---

## 🚀 Getting Started

```bash
git clone <repo-link>
cd WarpXfer
g++ main.cpp -o warpXfer
./warpXfer
