# Thread-Safe IPC Shared Memory Server

A multi-threaded C application demonstrating IPC (Inter-Process Communication) and network programming. The project implements a TCP server that manages a System V shared memory segment, allowing multiple connected clients to concurrently read, write, and clear shared data safely using POSIX mutex synchronization.

## Features

* **Multi-Threaded Architecture**: Spawns a dedicated thread per client connection using `pthread`[cite: 1].
* **System V Shared Memory**: Allocates and manages a shared memory segment (`shmget`, `shmat`) across IPC[cite: 1].
* **Thread Safety**: Synchronizes shared memory read/write operations with `pthread_mutex_t` to prevent race conditions[cite: 1].
* **Resource Cleanup**: Gracefully intercepts `SIGINT` and `SIGTERM` signals to detach and destroy shared memory resources[cite: 1].
* **Interactive CLI**: Simple command-line client for issuing commands to the server[cite: 2].

## Structure

* `lab1_server.c` — Multi-threaded TCP server handling shared memory operations and client requests[cite: 1].
* `lab1_client.c` — Interactive socket client for sending commands[cite: 2].

## Supported Commands

* `READ` — Retrieve and display the current contents of the shared memory[cite: 1, 2].
* `WRITE <data>` — Append string data to the shared memory segment[cite: 1, 2].
* `CLEAR` — Clear the contents of the shared memory segment[cite: 1, 2].
* `exit` — Close the client socket connection[cite: 2].

## Build & Run

### 1. Compilation
Compile the server and client using `gcc`:

```bash
gcc -pthread lab1_server.c -o server
gcc lab1_client.c -o client
