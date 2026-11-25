import socket

HOST = "127.0.0.1"
PORT = 9000

def main():
    requests = [
        f"ECHO Message-{i}" if i % 2 == 0 else "PING"
        for i in range(100)
    ]

    expected = [
        f"OK Message-{i}" if i % 2 == 0 else "PONG"
        for i in range(100)
    ]

    with socket.create_connection((HOST, PORT), timeout=5) as client:
        client.settimeout(10)

        # Send all 100 requests together.
        client.sendall(("".join(r + "\n" for r in requests)).encode())

        received = b""

        while received.count(b"\n") < len(requests):
            chunk = client.recv(4096)
            if not chunk:
                raise RuntimeError("Server closed connection unexpectedly")
            received += chunk

    responses = received.decode().strip().split("\n")

    if responses == expected:
        print(f"Received responses: {len(responses)}")
        print("PASS: All pipelined responses arrived in the correct order.")
    else:
        print("FAIL: Response order or response content is incorrect.")
        for i, (actual, wanted) in enumerate(zip(responses, expected)):
            if actual != wanted:
                print(f"Mismatch at request {i}: expected {wanted!r}, got {actual!r}")
                break
        raise AssertionError("Pipelining test failed")

if __name__ == "__main__":
    main()
