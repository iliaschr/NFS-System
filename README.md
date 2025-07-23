# Network File System (NFS) - Distributed File Synchronization

![run-tests](https://github.com/iliaschr/NFS-System/actions/workflows/ci.yml/badge.svg)

A high-performance, thread-safe distributed file system that automatically synchronizes files between multiple remote clients. Built in C with a focus on scalability and reliability.

## Key Features

- **Distributed Architecture**: Central manager coordinates file sync between multiple remote clients
- **Multi-threaded Design**: Producer-consumer pattern with configurable worker thread pool
- **Real-time Sync**: Automatic file synchronization with chunk-based transfers for large files
- **Thread-safe Operations**: Mutex-protected data structures and concurrent job processing
- **Custom Protocol**: Efficient text-based control protocol with binary data transfer
- **Comprehensive Testing**: Unit tests, integration tests, and memory leak detection

## Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ nfs_console │────│ nfs_manager │────│ nfs_client  │
│   (CLI)     │    │ (Coordinator)│    │(File Server)│
└─────────────┘    └─────────────┘    └─────────────┘
                           │
                   ┌───────┴───────┐
                   │ Worker Threads │
                   │   (Job Pool)   │
                   └───────────────┘
```

**Components:**
- **Manager**: Central coordinator with configurable thread pool
- **Console**: Interactive CLI for system control  
- **Client**: File server handling local operations
- **Workers**: Parallel job execution for high throughput

## Technical Implementation

- **Language**: C with POSIX threads
- **Networking**: TCP sockets for reliable communication
- **Data Structures**: Linked lists with mutex protection
- **Memory Management**: Careful resource cleanup and leak prevention
- **Error Handling**: Comprehensive logging with graceful degradation

## Quick Start

```bash
# Build the system
make all

# Start file servers
./nfs_client -p 8001
./nfs_client -p 8002

# Configure sync pairs (config.txt)
/source@127.0.0.1:8001 /target@127.0.0.1:8002

# Start manager
./nfs_manager -c config.txt -n 4 -p 8000

# Use console interface
./nfs_console -h 127.0.0.1 -p 8000
```

## Available Commands

- `add <source> <target>` - Add new sync pair
- `cancel <source>` - Stop synchronization
- `shutdown` - Graceful system shutdown

## Testing & Quality

```bash
make tests && make run-tests    # Unit tests
make valgrind-test             # Memory leak detection
./test_system.sh               # Integration testing
```

## Protocol Specification

**File Operations:**
- `LIST <directory>` - Enumerate files
- `PULL <filepath>` - Retrieve file content  
- `PUSH <filepath> <size> [data]` - Store file with chunking

**Features:**
- Efficient binary data transfer
- Chunk-based streaming for large files
- Error handling and recovery mechanisms

---

**Built with:** C, POSIX Threads, TCP Sockets, Make
