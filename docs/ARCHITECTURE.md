# System Architecture

## 1. Overview

The High-Performance Multithreaded TCP Server is built using an event-driven architecture with a dedicated event-loop thread and a pool of worker threads.

The server uses Linux non-blocking sockets, level-triggered `epoll`, and `eventfd` to handle multiple TCP connections efficiently while separating network I/O from request processing.

## 2. Main Components

### Event Loop

The main event-loop thread manages:

- Listening socket and incoming client connections.
- Non-blocking client sockets.
- The epoll instance and readiness notifications.
- Per-connection input and output buffers.
- Connection lifecycle and socket cleanup.
- Worker completion notifications.

The event loop uses `epoll_wait()` to monitor socket readiness without continuously polling every connection.

### Thread Pool

The thread pool executes request-processing tasks concurrently.

It uses:

- `std::thread` for worker threads.
- `std::mutex` for shared-state synchronization.
- `std::condition_variable` to notify workers.
- A task queue to distribute incoming work.
- A shutdown mechanism that stops workers and joins their threads.

Workers process requests independently of the event loop.

### eventfd Completion Notification

Workers do not directly write to or close client sockets.

Instead, after processing a request, a worker:

1. Creates a completion record containing the connection identifier, request sequence number, and response.
2. Pushes the completion record into a mutex-protected queue.
3. Signals the event loop through `eventfd`.

The event loop monitors this notification through epoll, retrieves completed responses, and performs the actual socket writes.

### Connection Management

Each client connection maintains its own state, including:

- Connection identifier.
- Input buffer.
- Output buffer.
- Pending request count.
- Request sequence numbers.
- Completed responses awaiting ordered delivery.
- Peer EOF state.

Connection identifiers are used instead of relying only on file descriptors. This prevents a late worker completion from being delivered to a different connection if the operating system reuses a file descriptor.

## 3. Request Processing Flow

1. A client establishes a TCP connection with the server.
2. The server accepts the connection and configures the socket for non-blocking I/O.
3. The client socket is registered with epoll.
4. When data becomes available, the event loop reads it.
5. Incoming bytes are accumulated and separated into newline-delimited requests.
6. Complete requests are submitted to the thread pool.
7. A worker processes the request and generates a response.
8. The worker adds a completion record to the shared completion queue and signals eventfd.
9. The event loop retrieves completed responses.
10. Responses are placed in request sequence order.
11. The event loop sends pending response data through the client socket.

This design keeps socket operations in the event-loop thread while allowing request processing to happen concurrently.

## 4. TCP and epoll Handling

The server uses:

- Non-blocking TCP sockets.
- Level-triggered epoll.
- `epoll_create1()` to create the epoll instance.
- `epoll_ctl()` to register and modify monitored descriptors.
- `epoll_wait()` to receive readiness events.
- `EPOLLIN` for incoming data.
- `EPOLLOUT` when pending response data needs to be written.
- `EPOLLRDHUP` to detect peer shutdown of its writing side.
- `EPOLLERR` and `EPOLLHUP` for socket error and hang-up handling.

The server handles incomplete TCP messages by buffering incoming data until a complete newline-delimited request is available.

## 5. Pipelining and Response Ordering

Multiple requests can be sent over the same TCP connection.

Each request receives a sequence number. Worker threads may finish requests in a different order from the order in which they were received.

To preserve protocol ordering:

1. Completed responses are stored using their sequence numbers.
2. The event loop checks which response is next in sequence.
3. Responses are appended to the output buffer in request order.
4. Socket writes are handled by the event loop.

This allows concurrent request processing while maintaining ordered responses for pipelined requests.

## 6. Connection Lifecycle

The server supports:

- Multiple requests over one persistent connection.
- Fragmented TCP messages.
- Multiple pipelined requests.
- Client disconnections.
- TCP half-close handling.
- Oversized request rejection.
- Large response delivery.

When a client closes its sending side, the server handles the remaining pending requests and responses before closing the connection when its pending work and output are complete.

## 7. Graceful Shutdown

The server handles both SIGINT and SIGTERM shutdown requests.

The shutdown process:

1. Receives the shutdown signal.
2. Stops accepting new client connections.
3. Shuts down the thread pool and waits for worker tasks to finish.
4. Clears remaining completion state as part of shutdown.
5. Closes active client sockets and server resources.
6. Releases the epoll and eventfd descriptors.
7. Exits after cleanup.

This prevents worker threads from being left running after server shutdown.

## 8. Current Limitations

This project demonstrates core event-driven networking and multithreaded request processing. It is an educational implementation and is not intended as a production-ready networking service.

Current limitations include:

- The worker task queue is unbounded.
- Explicit backpressure and admission control are not implemented.
- Connection and idle timeouts are not implemented.
- Additional overload and resource-exhaustion protection is needed.
- More extensive stress testing and performance profiling would be useful.

## 9. Future Improvements

Potential improvements include:

- Bounded worker queues.
- Backpressure and admission control.
- Connection and idle timeouts.
- Improved socket error reporting.
- Runtime metrics and monitoring.
- More extensive stress and failure testing.
- Performance profiling and optimization.