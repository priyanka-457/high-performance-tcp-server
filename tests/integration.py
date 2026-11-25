#!/usr/bin/env python3
"""Basic integration checks. Start tcp_server before running this script."""
import socket
import concurrent.futures
import sys

HOST = "127.0.0.1"
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9000

def request(message):
    with socket.create_connection((HOST, PORT), timeout=5) as sock:
        sock.sendall(message.encode() + b"\n")
        data = b""
        while not data.endswith(b"\n"):
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk
        return data.decode().strip()

assert request("PING") == "PONG"
assert request("ECHO hello") == "OK hello"
assert request("BAD") == "ERR unknown command"
with concurrent.futures.ThreadPoolExecutor(max_workers=20) as executor:
    results = list(executor.map(lambda i: request(f"ECHO {i}"), range(50)))
assert results == [f"OK {i}" for i in range(50)]
print("PASS: PING, ECHO, invalid command, and 50 concurrent client requests")
