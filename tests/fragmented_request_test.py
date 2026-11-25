import socket
import time

HOST = "127.0.0.1"
PORT = 9000

with socket.create_connection((HOST, PORT), timeout=5) as client:
    client.settimeout(5)

    client.sendall(b"PI")
    time.sleep(0.2)
    client.sendall(b"NG\n")

    response = client.recv(1024)

    print("Received:", response.decode().strip())

    if response == b"PONG\n":
        print("PASS: Server handled a fragmented TCP request.")
    else:
        print("FAIL: Unexpected response.")
        raise SystemExit(1)
