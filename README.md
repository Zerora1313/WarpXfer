# 🚀 WarpXfer - High-Performance local P2P File Transfer Utility

WarpXfer is a high-performance, cross-platform, peer-to-peer (P2P) local file and folder transfer application written in **Modern C++**. It allows quick and secure direct transfers between devices on the same local area network (LAN/Wi-Fi) without relying on intermediate servers.

---

## ✨ Key Features
- **Zero-Configuration Discovery:** Seamless peer matching using local network UDP broadcasts (port `9000`), automatically displaying online receivers with funny generated usernames.
- **Adaptive Chunk Size Sizing:** Measures round-trip latency (RTT) dynamically using a ping-pong handshake and matches transmission block size (ranging from 256 KB to 8 MB) to network conditions.
- **Stateful Transfer Resuming:** Automatically detects partially transferred files, calculates exact offsets, and resumes file transmission from the point of interruption.
- **On-the-fly (Lazy) SHA-256 Hashing:** Native C++ dependency-free SHA-256 implementation that computes hash contexts inline during disk read operations, avoiding startup delays for large file transfers.
- **High Network Optimization:** Maximizes socket throughput via kernel buffer tuning (4 MB send/receive buffers), disabling Nagle's algorithm (`TCP_NODELAY`), and using persistent file streams to reduce kernel I/O system calls.

---

## 🛠 Tech Stack
- **Language:** Modern C++ (C++17 / C++20)
- **Networking APIs:** Socket Programming (Winsock2 for Windows / standard POSIX sockets for Linux/Unix)
- **Protocols:** TCP/IP (reliable data pipe) and UDP (peer auto-discovery broadcast)
- **Build System:** Makefile / GCC Compiler Helper Script (`warp.bat`)

---

## ⚙️ How It Works
1. **Discovery:** The sender broadcasts a `DISCOVER_REQ` UDP packet. Active receivers listen and reply back with their TCP port, IP, and alias name.
2. **RTT Ping:** The sender runs a ping-pong measurement against the selected receiver to pick the optimal chunk size.
3. **Metadata & Handshake:** The sender transmits files metadata. The receiver checks local storage to determine if any file is partially downloaded and replies with the matching byte offset.
4. **Streaming & Verification:** The sender reads chunks from disk, updates the running SHA-256 context in memory, and writes to TCP. The receiver updates its running SHA-256 context, writes to disk, and sends ACKs.
5. **Finalization:** The sender sends a `FILE_HASH_UPDATE` packet with the final hash. The receiver finalizes its context, matches hashes, and commits the file.

---

## 🚀 Getting Started

### Prerequisites
- GCC Compiler (`g++`) with support for C++17 or later.
- On Windows: MinGW or MSVC environment.

### Compiling
To compile the project into `warp.exe` (or `warp` binary):

**On Windows (using GCC/MinGW):**
Simply run the helper batch script:
```cmd
warp.bat
```

**Using Makefile (cross-platform):**
```bash
make
```

---

## 💻 Usage

```text
Usage:
  warp send <file_or_folder> [options]
  warp receive [options]
  warp --help | -h

Options:
  --port <port>       Port to use for TCP data transfer (default: 9001)
  --manual <ip>       Directly connect to receiver IP (skips UDP discovery)
  --dir <path>        Directory to save files (for receiver, default: current directory)
  --udpport <port>    UDP port used for auto-discovery (default: 9000)
```

### Examples

**Receiver:**
Run receiver and save incoming files inside a custom folder:
```bash
warp receive --dir C:\Downloads\warp-received\
```

**Sender (using auto-discovery scan):**
Send a school project folder to a scanned receiver:
```bash
warp send D:\schoolProject\
```

**Sender (manual direct IP):**
Directly send a movie file to a receiver at `192.168.1.10`:
```bash
warp send D:\movie.mp4 --manual 192.168.1.10
```
