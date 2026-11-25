#!/usr/bin/env python3
"""Small interactive TCP client for the project's newline-delimited protocol."""
import socket
import sys

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 9000

with socket.create_connection((host, port), timeout=5) as sock:
    print("Connected. Enter PING or ECHO <text>. Type quit to exit.")
    reader = sock.makefile("r", encoding="utf-8", newline="\n")
    while True:
        try:
            line = input("> ")
        except EOFError:
            break
        if line.lower() == "quit":
            break
        sock.sendall((line + "\n").encode())
        response = reader.readline()
        if not response:
            print("Server closed the connection.")
            break
        print(response.rstrip("\n"))
