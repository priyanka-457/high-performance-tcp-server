
# High-Performance Multithreaded TCP Server

A high-performance, event-driven TCP server implemented in C++17 on Linux using POSIX sockets, non-blocking I/O, epoll, and a multithreaded worker pool.

This project demonstrates practical systems programming concepts, including TCP connection management, concurrent request processing, asynchronous I/O handling, and graceful server shutdown.

## Features

- TCP server implementation using POSIX sockets
- Non-blocking socket operations
- Event-driven I/O using Linux epoll (level-triggered)
- Multithreaded worker pool using C++ threads
- Thread synchronization using mutex and condition_variable
- eventfd-based worker completion notification
- Support for multiple simultaneous TCP clients
- Multiple requests over a persistent TCP connection
- Fragmented and partial TCP message handling
- Ordered responses for pipelined requests
- TCP half-close handling
- Client disconnection handling
- Oversized request detection and rejection
- Large response handling
- Graceful shutdown using SIGINT and SIGTERM

## Technologies Used

| Technology | Purpose |
|---|---|
| C++17 | Core server implementation |
| Linux / Ubuntu / WSL | Development environment |
| TCP/IP | Network communication |
| POSIX Sockets | TCP socket programming |
| epoll | Event-driven I/O |
| eventfd | Worker completion notification |
| std::thread | Multithreading |
| mutex and condition_variable | Thread synchronization |
| CMake | Build configuration |
| Python 3 | Client, testing and benchmarking |
| Git | Version control |

## Project Structure

```text
High-Performance-TCP-Server/
│
├── include/
│   └── thread_pool.hpp
│
├── src/
│   └── main.cpp
│
├── client/
│   └── client.py
│
├── tests/
│   ├── integration.py
│   ├── concurrent_clients_test.py
│   ├── multiple_requests_test.py
│   ├── pipelining_test.py
│   ├── fragmented_request_test.py
│   ├── partial_message_test.py
│   ├── half_close_test.py
│   ├── disconnect_test.py
│   ├── oversized_request_test.py
│   └── large_response_test.py
│
├── benchmark/
│   └── benchmark.py
│
├── docs/
│   ├── ARCHITECTURE.md
│   └── PROTOCOL.md
│
├── screenshots/
│
├── CMakeLists.txt
├── README.md
└── .gitignore
```

## Build Instructions

### Prerequisites

- Ubuntu Linux or WSL
- GCC / G++ with C++17 support
- CMake
- Python 3

Install dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake python3
```

### Configure and Build

Navigate to the project directory:

```bash
cd High-Performance-TCP-Server
```

Configure the project:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build -j4
```

## Running the Server

Start the TCP server with 8 worker threads:

```bash
./build/tcp_server 9000 8
```

Expected output:

```text
TCP server listening on 0.0.0.0:9000 with 8 worker threads
```

The server listens on TCP port 9000.

### Connecting a Client

Open another terminal and run:

```bash
python3 client/client.py 127.0.0.1 9000
```

### Supported Commands

| Request | Expected Response |
|---|---|
| PING | PONG |
| ECHO Hello | OK Hello |
| Invalid command | Error response |

Enter `quit` in the client to disconnect.

Press `Ctrl+C` in the server terminal to initiate graceful shutdown.

## Testing

Run the tests while the server is running.

```bash
python3 tests/integration.py
python3 tests/concurrent_clients_test.py
python3 tests/multiple_requests_test.py
python3 tests/pipelining_test.py
python3 tests/fragmented_request_test.py
python3 tests/partial_message_test.py
python3 tests/half_close_test.py
python3 tests/disconnect_test.py
python3 tests/oversized_request_test.py
python3 tests/large_response_test.py
```

### Verified Test Results

The following scenarios have been tested successfully:

- PING and ECHO request processing
- Invalid command handling
- 50 concurrent client requests through the integration test
- 500 simultaneous TCP clients
- Multiple requests over one TCP connection
- 100 pipelined requests with ordered responses
- Fragmented TCP message handling
- Partial message handling
- TCP half-close handling
- Client disconnection and reconnection
- Oversized request rejection
- Large TCP response delivery
- Graceful SIGINT shutdown
- Graceful SIGTERM shutdown

### Sanitizer Testing

The project was also built and tested using:

- AddressSanitizer (ASan)
- UndefinedBehaviorSanitizer (UBSan)
- ThreadSanitizer (TSan)

Functional tests were run against the sanitizer builds. No sanitizer diagnostics were shown in the tested runs.

## Performance Benchmark

A Python-based benchmark script measures server throughput and request latency.

Run:

```bash
python3 benchmark/benchmark.py --clients 10 --requests 1000
```

### Recorded Benchmark Results

The following results were measured on the local development environment using localhost TCP connections.

| Concurrent Clients | Total Requests | Throughput |
|---:|---:|---:|
| 10 | 1,000 | 2,326.12 requests/sec |
| 50 | 5,000 | 2,411.46 requests/sec |
| 100 | 10,000 | 2,086.96 requests/sec |
| 200 | 20,000 | 2,307.32 requests/sec |

### Latest Benchmark: 200 Concurrent Clients

```text
completed=20000
failures=0
elapsed_s=8.668

throughput_req_s=2307.32

avg_ms=76.735
median_ms=72.020
p95_ms=155.293
p99_ms=201.681
```

### Benchmark Methodology

- Clients connect through localhost.
- Each benchmark request creates a new TCP connection.
- The benchmark measures completed requests, failures, elapsed time, throughput, and latency percentiles.
- Results represent local development measurements, not production or internet performance guarantees.
- Performance can vary depending on system resources and workload.

## Linux Networking Diagnostics

The following Linux commands are useful for inspecting and troubleshooting the server:

Check listening TCP sockets:

```bash
ss -lntp
```

Inspect established TCP connections:

```bash
ss -tnp
```

Inspect running processes:

```bash
ps -ef | grep tcp_server
```

Monitor CPU and memory:

```bash
top
```

## Architecture and Protocol Documentation

Additional technical documentation:

- [System Architecture](docs/ARCHITECTURE.md)
- [TCP Protocol](docs/PROTOCOL.md)

## Limitations and Future Improvements

This project is an educational systems-programming implementation and is not intended to be a production-ready networking service.

Potential future improvements include:

- Bounded worker task queues
- Backpressure and overload protection
- Connection and idle timeouts
- Improved socket error reporting
- Detailed runtime metrics and monitoring
- More extensive stress and failure testing
- Performance profiling and optimization

## Learning Outcomes

Through this project, I gained practical experience with:

- Linux network programming
- TCP socket lifecycle and connection management
- Event-driven programming using epoll
- Multithreading and synchronization in C++
- Concurrent client request processing
- TCP stream framing and partial message handling
- Graceful shutdown and resource cleanup
- Linux networking diagnostics
- Performance benchmarking and latency analysis

## Author

Developed as a systems programming project to strengthen practical knowledge of C++ concurrency, Linux networking, and high-performance server architecture.
