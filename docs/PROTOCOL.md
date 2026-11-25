
# Application Protocol

This project implements a custom TCP request/response protocol. It is not an HTTP server.

## 1. Message Format

- Messages use UTF-8 text.
- Each request must end with a newline character (`\n`).
- Each response also ends with a newline character (`\n`).
- TCP is a byte stream, so a request may arrive across multiple reads, or multiple requests may arrive in a single read.
- The server buffers incoming data until it receives a complete newline-terminated request.

## 2. Supported Commands

| Request | Response |
|---|---|
| `PING` | `PONG` |
| `ECHO <text>` | `OK <text>` |
| Any other non-empty command | `ERR unknown command` |
| Empty line | `ERR empty request` |

### Examples

Request:
```text
PING
```

Response:
```text
PONG
```

Request:
```text
ECHO Hello World
```

Response:
```text
OK Hello World
```

Request:
```text
STATUS
```

Response:
```text
ERR unknown command
```

## 3. Connection Handling

- The server supports multiple simultaneous TCP clients.
- A client can send multiple requests over the same connection.
- Requests can arrive in fragments or be pipelined together.
- The server uses non-blocking sockets and level-triggered `epoll`.
- Readable sockets are processed until `EAGAIN` or `EWOULDBLOCK`.
- Partial writes are handled by retaining unsent response data until the socket is writable again.
- An unfinished input buffer is limited to 64 KiB. Requests exceeding this limit are rejected with `ERR request too large`.

## 4. Request Processing and Response Ordering

Each complete request is submitted to the worker thread pool for processing.

Worker threads may finish requests in a different order from the order in which they were received. However, the server uses per-connection sequence numbers and buffers completed responses to preserve request order.

**Responses are sent in the same order as requests were received on that connection, even if worker threads finish processing them in a different order.**

## 5. Error Handling

The server handles:
- Unknown commands.
- Empty requests.
- Oversized requests.
- Client disconnections.
- TCP half-close events.
- Socket read and write errors.

An incomplete request that has not been terminated by a newline is not processed as a complete command.

## 6. Protocol Limitations

- This is a plain-text TCP protocol, not HTTP.
- There is no HTTP request format, status code, or header support.
- There is no authentication or encryption built into the application protocol.
- Clients must send newline-terminated requests and read newline-terminated responses.
