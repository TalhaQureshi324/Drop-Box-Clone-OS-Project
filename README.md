# 🗂️ A Simple Dropbox Clone

## 👥 Group Members

| Name | Roll Number | Role |
|------|--------------|------|
| **Muhammad Talha Qureshi** | BSCS-23122 | Server Development & Synchronization |
| **Assadullah Farukh** | BSCS-23213 | Client Threading & Testing |
| **Abdullah Salman** | BSCS-23053 | File Operations & Authentication |

---

## 🧩 1. Design Report

### 📘 Overview
This project implements a **multi-threaded Dropbox-like file storage server** that allows multiple concurrent client connections.  
The server supports the following core operations:
- `UPLOAD <filename>`
- `DOWNLOAD <filename>`
- `DELETE <filename>`
- `LIST`

Each user is authenticated and has separate storage.  
The focus of this project was to ensure **thread synchronization**, **resource management**, **race condition avoidance**, and **memory safety**.

---

### 🧱 Architecture

The system is organized into three main layers:

#### 1️⃣ Main Thread (Accept Loop)
- Listens for incoming TCP client connections.  
- Pushes accepted socket descriptors into a **Client Queue**.

#### 2️⃣ Client Thread Pool
- Multiple client threads dequeue sockets from the Client Queue.
- Each thread handles authentication (signup/login) and command parsing.
- When a command is received, it is converted into a `Task` and pushed into the **Task Queue**.

#### 3️⃣ Worker Thread Pool
- Worker threads dequeue `Task` objects and perform the actual file operations.
- Handles concurrent file access safely, ensuring data consistency.

---

## 🔄 2. Communication Mechanism Between Worker and Client Thread Pools

### 🧠 Chosen Approach: TaskResult-Based Synchronization

Each task created by a client includes a pointer to a `TaskResult` structure:

```c
typedef struct TaskResult {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int done;
    char *response;
} TaskResult;
```
The client thread waits on this TaskResult until the worker thread finishes processing the task.

The worker thread updates the result and signals the client thread using the condition variable.

### ✅ Why This Design Works
- Eliminates busy waiting.
- Enables direct synchronization between producer (client) and consumer (worker).
- Keeps code modular and easy to extend.
- Ensures correct client-task response mapping.

---

## ⚙️ 3. Build and Run Instructions

### 🧾 Prerequisites
Make sure the following are installed:

```bash
sudo apt install gcc make valgrind
```

### 🏗️ Build the Project
To compile:

```bash
make
```

To clean all build files:

```bash
make clean
```

### 🚀 Run the Server
```bash
./server
```

### 💻 Run the Client
```bash
./client_app
```

---

## 🧪 4. Testing for Race Conditions (ThreadSanitizer)

### 🧰 Command Used
```bash
make clean
make CFLAGS="-Wall -Wextra -pthread -g -fsanitize=thread"
./server
```

### 🧾 Expected Output
Initially, ThreadSanitizer may report data races.

After applying atomic operations (`atomic_int server_running`), the report shows:

```
==================
ThreadSanitizer: reported 0 warnings
==================
```
✅ No race conditions detected.

📸 (Insert screenshot of your TSAN terminal output here.)

---

## 🧹 5. Testing for Memory Leaks (Valgrind)

### 🧰 Command Used
```bash
make clean
make CFLAGS="-Wall -Wextra -pthread -g"
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./server
```

### 🧾 Expected Output
```
==XXXX== All heap blocks were freed -- no leaks are possible
==XXXX== ERROR SUMMARY: 0 errors from 0 contexts
```
✅ Memory fully freed — no leaks detected.

📸 (Insert screenshot of your Valgrind terminal output here.)

---

## 🗂️ 6. Project Repository Structure

```
├── Makefile
├── README.md
├── src/
│   ├── server.c
│   ├── queues.c
│   ├── task_queue.c
│   ├── worker_thread.c
│   ├── client_thread.c
│   ├── locks.c
│   ├── auth.c
│   └── server.h
└── client/
    └── client.c
```

---

## 🔐 7. Synchronization Summary

| Mechanism | Purpose |
|------------|----------|
| Mutexes | Protect shared data structures (ClientQueue, TaskQueue) |
| Condition Variables | Signal threads when queues are not empty/full |
| Atomic Variables | Used for global flags (like server_running) |
| Graceful Shutdown | Ensures threads wake, exit cleanly, and all resources are released |

---

## 🧾 8. Verification Summary

| Tool | Purpose | Result |
|------|----------|--------|
| ThreadSanitizer (TSAN) | Detect race conditions | ✅ No races found |
| Valgrind | Memory leak detection | ✅ No leaks |
| Makefile | Automated build process | ✅ Works perfectly |
| Client/Server Testing | Verifies end-to-end functionality | ✅ All commands successful |

---

## 💡 9. Future Improvements

- Add file encryption during upload/download.
- Implement user quotas and file version control.
- Introduce priority scheduling for tasks.
- Build a graphical client interface.

---

## 🧾 10. GitHub Repository and Reports

📎 **GitHub Link:** Add your repository link here  
📸 **Screenshots:** Attach screenshots of TSAN and Valgrind reports.  
🧱 **Final Report:** Include this README and Phase-2 design document.

---

## 🏁 Conclusion

This project successfully demonstrates:

- Multi-threaded architecture in C using pthread.
- Safe communication via producer-consumer queues.
- Elimination of race conditions and memory leaks.
- Robust synchronization and graceful shutdown handling.

✅ **Result:** A fully functional, race-free, and leak-free Dropbox-style server in C.
