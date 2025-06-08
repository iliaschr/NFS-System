![run-tests](https://github.com/disyspro/hw2-iliaschr/actions/workflows/ci.yml/badge.svg)

# Network File System (NFS) - Distributed File Synchronization

A high-performance, thread-safe distributed network file system implementing automatic file synchronization between multiple remote clients.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Design Decisions](#design-decisions)
- [Component Documentation](#component-documentation)
- [Build Instructions](#build-instructions)
- [Usage Guide](#usage-guide)
- [Protocol Specification](#protocols)
- [Testing](#testing)
- [Performance Considerations](#performance-considerations)
- [Error Handling](#error-handling)

## Overview

This NFS implementation provides automated file synchronization between distributed clients through a central manager component. The system uses a multi-threaded architecture with producer-consumer patterns to achieve high throughput and scalability.


## Architecture

### Component Relationships

1. **nfs_manager**: Central coordinator managing synchronization
2. **nfs_console**: User interface for issuing commands
3. **nfs_client**: File server handling local file operations
4. **Worker Threads**: Parallel execution units for file transfers

## Design Decisions

**Decision**: Split nfs_client to nfs_client_logic.c and nfs_client.c because
I got errors when testing with acutest. 

### 3. Data Structure Choices

#### Sync Info Store: Linked List
**Decision**: Simple linked list with mutex protection.

**Rationale**:
- Small expected number of sync pairs (typically < 100)
- Simple implementation with clear thread safety
- O(n) search acceptable for this scale
- Easy to implement and debug

#### Job Queue: Linked List with Tail Pointer
**Decision**: FIFO queue using linked list with head/tail pointers.

**Rationale**:
- O(1) enqueue and dequeue operations
- Natural FIFO ordering for job processing
- Simple implementation with bounded capacity
- Efficient memory usage (no pre-allocation)

### 4. Error Handling Philosophy

**Decision**: Fail-fast with comprehensive logging and graceful degradation.

**Rationale**:
- Early error detection prevents cascade failures
- Detailed logging aids debugging and monitoring
- Individual file failures don't stop entire sync process
- System remains operational with partial failures

**Implementation**:
- Consistent error checking with `strerror()` usage
- Structured logging with timestamps
- Error isolation per sync job
- Recovery mechanisms for transient failures

### 5. Protocol Design

**Decision**: Simple text-based protocol with binary data transfer.

**Rationale**:
- Human-readable commands aid debugging
- Efficient binary transfer for file data
- Simple parsing reduces complexity
- Clear separation of control and data planes

**Commands**:
- `LIST <directory>`: Enumerate files
- `PULL <filepath>`: Retrieve file content
- `PUSH <filepath> <chunksize> [data]`: Store file content

### 6. Synchronization Strategy

**Decision**: Pull-then-Push with chunk-based transfer.

**Rationale**:
- Manager coordinates all transfers (central control)
- Chunk-based transfer supports large files
- Memory-efficient streaming (bounded buffer usage)
- Progress tracking possible for future enhancement

**Flow**:
1. Manager connects to source client
2. Retrieves file using PULL command
3. Simultaneously streams to target using PUSH
4. Handles chunks to manage memory usage

## Component Documentation

### nfs_manager

**Purpose**: Central coordination and job scheduling

**Key Responsibilities**:
- Parse configuration files and command-line arguments
- Manage worker thread pool lifecycle
- Handle console connections and command processing
- Coordinate file synchronization between clients
- Maintain sync pair status and error tracking

**Threading Model**:
- Main thread: Accept console connections
- Worker threads: Process sync jobs
- Signal handler: Graceful shutdown coordination

### nfs_console

**Purpose**: User interface for system control

**Key Features**:
- Interactive command-line interface
- Command validation and parsing
- Connection management to manager
- Command logging with timestamps

**Supported Commands**:
- `add <source> <target>`: Add sync pair
- `cancel <source>`: Cancel synchronization
- `shutdown`: Stop manager gracefully
- `help`: Display available commands

### nfs_client

**Command Handlers**:
- LIST: Directory enumeration
- PULL: File content retrieval
- PUSH: File content storage

## Build Instructions

### Prerequisites

- make utility
- Valgrind for memory testing

### Compilation

```bash
# Build all components
make all

# Build with debug symbols
make debug

# Build optimized release version
make release

# Build and run tests
make tests
make run-tests

# Memory leak testing (requires Valgrind)
make valgrind-test
```

### Build Targets

- `all`: Build all main executables (default)
- `debug`: Debug build with symbols and no optimization
- `release`: Optimized release build
- `tests`: Build unit test executables
- `clean`: Remove all build artifacts
- `help`: Display available targets

## Usage Guide

### Basic Setup

1. **Start File Servers**:
```bash
# Terminal 1: Source file server
./nfs_client -p 8001

# Terminal 2: Target file server  
./nfs_client -p 8002
```

2. **Create Configuration**:
```bash
# config.txt
/source_directory@127.0.0.1:8001 /target_directory@127.0.0.1:8002
```

3. **Start Manager**:
```bash
./nfs_manager -l manager.log -c config.txt -n 3 -p 8000 -b 10
```

4. **Start Console**:
```bash
./nfs_console -l console.log -h 127.0.0.1 -p 8000
```

### Command Reference

#### nfs_manager
```bash
./nfs_manager -l <logfile> -c <config> -n <workers> -p <port> -b <buffer_size>
```

Parameters:
- `-l`: Manager log file path
- `-c`: Configuration file path
- `-n`: Number of worker threads (default: 5)
- `-p`: TCP port for console connections
- `-b`: Maximum job queue size

#### nfs_console
```bash
./nfs_console -l <logfile> -h <host> -p <port>
```

Parameters:
- `-l`: Console log file path
- `-h`: Manager hostname/IP
- `-p`: Manager port number

#### nfs_client
```bash
./nfs_client -p <port>
```

Parameters:
- `-p`: TCP port to listen on

### Configuration File Format

```
# Comments start with #
/path1@host1:port1 /path2@host2:port2
/documents@192.168.1.10:8001 /backup@192.168.1.20:8002
```

Each line specifies a source-target sync pair in format:
`/source_path@source_host:source_port /target_path@target_host:target_port`

## Protocols

### Client Protocol

#### LIST Command
```
Request:  LIST /directory/path
Response: file1.txt\nfile2.txt\n.\n
```

#### PULL Command
```
Request:  PULL /path/to/file.txt
Response: <filesize> <binary_data>
Error:    -1 <error_message>
```

#### PUSH Command
```
Start:    PUSH /path/to/file.txt -1
Chunk:    PUSH /path/to/file.txt <size> <binary_data>
End:      PUSH /path/to/file.txt 0
```

### Console Protocol

#### Commands
- `add /source@host:port /target@host:port`
- `cancel /source@host:port`
- `shutdown`

#### Responses
- Success/error messages with timestamps
- Status information for operations

## Testing

### Unit Tests

```bash
# Run all unit tests
make run-tests

# Individual test suites
./test_utils        # Utility function tests
./test_nfs_client   # Client logic tests
```

### Integration Testing

```bash
# Comprehensive system test
./test_system.sh

# Manual integration test
make demo
```

### Memory Testing

```bash
# Valgrind leak detection
make valgrind-test

# Individual component testing
make valgrind-client
make valgrind-manager
make valgrind-console
```

### Test Coverage

- **Unit Tests**: Core functions and data structures
- **Integration Tests**: Component interaction
- **Memory Tests**: Leak detection and validation
- **Error Tests**: Failure scenario handling
- **Performance Tests**: Load and stress testing


## Error Handling

### Logging Format

```
[YYYY-MM-DD HH:MM:SS] [source@host:port] [target@host:port] [thread_id] [operation] [result] [details]
```

Example:
```
[2025-06-08 10:15:30] [/docs@192.168.1.1:8001] [/backup@192.168.1.2:8002] [1234] [PULL] [SUCCESS] [1024 bytes pulled]
```
