import socket

HOST = "127.0.0.1"
PORT = 9000
payload = "A" * 60000
expected = f"OK {payload}\n".encode()

with socket.create_connection((HOST, PORT), timeout=5) as client:
    client.settimeout(10)
    client.sendall(f"ECHO {payload}\n".encode())

    received = bytearray()
    while len(received) < len(expected):
        chunk = client.recv(8192)
        if not chunk:
            break
        received.extend(chunk)

if bytes(received) == expected:
    print(f"Expected response size: {len(expected)} bytes")
    print(f"Received response size: {len(received)} bytes")
    print("PASS: Large TCP response received completely.")
else:
    print(f"Expected: {len(expected)} bytes")
    print(f"Received: {len(received)} bytes")
    print("FAIL: Response was incomplete or incorrect.")
    raise SystemExit(1)
