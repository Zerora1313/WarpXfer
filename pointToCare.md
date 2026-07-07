# WarpXfer: Transfer Speed & bottlenecks guide

WarpXfer project ki file transfer speed aur uske bottlenecks ko samajhne ke liye iske code design ko samajhna zaroori hai. Chaliye isko simple points me dekhte hain:

---

## 1. Core Working Principle (Ye kaise kaam karta hai)

WarpXfer transfer ke liye **TCP sockets** ka use karta hai aur application layer pe ek custom **Stop-and-Wait (Synchronous)** protocol implement karta hai:

* **Chunking (Tukde banana):** Pura file ek baar me nahi bheja jata. Sender file ko **8 KB (8192 bytes)** ke chhote-chhote packets (chunks) me divide karta hai.
* **Send -> Hash -> Write -> ACK Cycle:**
  * **Sender:** Disk se 8 KB read karta hai, `FILE_CHUNK` packet banata hai, aur TCP socket pe write kar deta hai. Uske baad sender **ruk jata hai (block ho jata hai)** aur receiver ke response ka wait karta hai.
  * **Receiver:** Network se 8 KB packet receive karta hai.
  * **SHA-256 Verification:** Receiver us chunk ka **SHA-256 hash** calculate karta hai verify karne ke liye ki data corrupt toh nahi hua.
  * **Disk Write:** Receiver us chunk ko direct disk pe file me write karta hai.
  * **ACK Send:** Sab check hone ke baad, receiver ek `CHUNK_ACK` packet sender ko wapas bhejta hai.
  * **Sender Resume:** Sender jab `CHUNK_ACK` receive karta hai, tabhi wo agli 8 KB chunk padhta hai aur bhejta hai.

---

## 2. Speed Bottlenecks (Speed kam hone ke main reasons)

WarpXfer ki speed lagbhag **0.4 MB/s se 0.5 MB/s** hi aati hai. Wi-Fi fast hone ke baad bhi speed kam hone ke peeche ye factors hain:

### 🚨 A. Stop-and-Wait Latency (Sabse bada bottleneck)
* **Problem:** Har 8 KB data bhejne ke baad sender ko receiver ke ACK ka wait karna padta hai. Iska matlab hai ki transmission speed network ki **Latency (Ping/RTT - Round Trip Time)** pe direct depend karti hai.
* **Maths:** Agar aapke Wi-Fi me ping (latency) **10 ms** hai:
  * 1 second me maximum $100$ round-trips (packets + ACKs) ho sakte hain.
  * Maximum speed = $100 \times 8\text{ KB} = 800\text{ KB/s}$ (approx **0.8 MB/s**).
  * Is wajah se network bandwidth jitni marzi ho (chahe 1000 Mbps ho), speed ping latency se cap ho jati hai.

### 📦 B. Very Small Chunk Size (8 KB)
* **Problem:** 8 KB ka size modern computers aur routers ke liye bohot chhota hai. Chhote chunks hone se TCP packets ka header overhead badh jata hai, aur C++ code me system calls (`send`/`recv`) bohot zyada baar call hote hain, jisse CPU overhead badhta hai.

### 🔑 C. Per-Chunk SHA-256 Calculation
* **Problem:** Receiver har single 8 KB chunk par SHA-256 cryptographic hash calculate karta hai. CPU ko 8-8 KB ke data par baar-baar hashing algorithm chalana padta hai, jo data transfer ke dauran high CPU usage aur processing delay create karta hai.

### 💾 D. Synchronous Disk I/O
* **Problem:** Data write hone ke baad hi ACK bheja jata hai. Jab tak disk 8 KB write complete nahi karti, network idle rehta hai. Isme buffering ya asynchronous queueing (jaise background thread me write karna) use nahi ho rahi hai.

---

## 🚀 Speed badhane ke liye kya sudhaar kiya ja sakta hai?

1. **Chunk Size badhana:** 8 KB ko badha kar **1 MB** ya **4 MB** karne se latency ka asar bohot kam ho jayega aur speed 10x-50x badh jayegi.
2. **Pipelining / Sliding Window:** Bina har chunk ke ACK ka wait kiye ek sath multiple chunks bhej dena, aur receiver unka validation background me kare.
3. **Hashing Optimizer:** Har chunk ka alag SHA-256 calculate karne ke bajaye, pooray file ka ek sath end-to-end hash verify karna.
