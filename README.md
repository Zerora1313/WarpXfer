# 🚀 WarpXfer - High-Performance P2P Local File Transfer Utility

WarpXfer is an **easy-to-use, user-friendly, and blazing fast command-line interface (CLI) tool** written in **Modern C++** for direct peer-to-peer (P2P) file and folder transfers across local area networks (LAN/Wi-Fi). 

By establishing direct sockets between devices, it eliminates the need for any cloud servers, internet bandwidth, or registration, providing secure and localized file sharing at the physical speed limit of your local connection.

---

## ✨ Key Features
- **Zero-Configuration Auto-Discovery:** Instant serverless peer discovery using UDP broadcasts (port `9000`). It scans the local network and displays online receivers with dynamically generated funny aliases.
- **Adaptive Chunk Size Sizing:** Automatically runs a ping-pong latency check (RTT) before session startup and adjusts the block transfer size (from 256 KB to 8 MB) to match network signal strength.
- **Stateful Resume Protocol:** Scans target paths for partially downloaded files, negotiates byte offsets via a handshake, and resumes transmission from the point of interruption.
- **On-the-fly (Lazy) SHA-256 Hashing:** Features a native, dependency-free SHA-256 implementation that updates hash bytes inline as blocks are read from disk. This removes upfront hash generation freezes for large folders.

---

## 🛠 Tech Stack
- **Language:** Modern C++ (C++17 / C++20)
- **Networking APIs:** Socket Programming (Winsock2 for Windows / standard POSIX sockets for Linux/Unix)
- **Protocols:** TCP/IP (reliable data pipe) and UDP (peer auto-discovery broadcast)
- **Build System:** Makefile / GCC Compiler Helper Script (`warp.bat`)

---

## ⚙️ How It Works
1. **Discovery:** The receiver listens on UDP; the sender broadcasts a `DISCOVER_REQ`. The receiver responds with its connection parameters.
2. **Latency Check:** The sender pings the receiver to compute RTT and select the chunk size.
3. **Offset Handshake:** Receiver reports local partial file sizes so the sender can seek (`seekg`) and resume.
4. **Optimized Transfer:** Sender streams blocks and updates the SHA-256 context inline. Receiver writes incoming chunks to disk using a persistent file handle to minimize write overhead.
5. **Hash Validation:** Once the last block is written, the receiver compares its final hash against the sender's. If they match, the file is saved; otherwise, it is deleted and re-requested.

---

## 🚀 Getting Started

### Prerequisites
- GCC Compiler (`g++`) with support for C++17 or later.
- On Windows: MinGW or MSVC environment.

### Compiling
To build the project into `warp.exe` (or `warp` binary):

**On Windows (using GCC/MinGW):**
Run the helper script:
```cmd
warp.bat
```

**Using Makefile (cross-platform):**
```bash
make
```

---

## 💡 Pro Tip: Run WarpXfer from Anywhere (Environment PATH Setup)

Instead of copying `warp.exe` to every folder you want to transfer, add it to your system's environment variables to run it system-wide from any terminal folder.

### On Windows (PowerShell):
1. Create a dedicated folder (e.g., `C:\Program Files\WarpXfer` or a custom directory inside your profile) and move your compiled `warp.exe` there.
2. Run PowerShell as **Administrator** and execute the following command to add it to the User PATH:
   ```powershell
   [System.Environment]::SetEnvironmentVariable("PATH", [System.Environment]::GetEnvironmentVariable("PATH", "User") + ";C:\Path\To\Your\WarpXfer\Folder", "User")
   ```
3. Restart your terminal. You can now run `warp send` or `warp receive` from any directory!

### On Linux / macOS:
1. Move the compiled `warp` binary to `/usr/local/bin` (requires root privileges):
   ```bash
   sudo cp warp /usr/local/bin/
   sudo chmod +x /usr/local/bin/warp
   ```
2. Now, you can invoke `warp` from any directory in your terminal!

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
