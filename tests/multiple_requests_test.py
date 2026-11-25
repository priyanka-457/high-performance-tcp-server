
import socket

HOST = "127.0.0.1"
PORT = 9000

def receive_line(client):
    response = b""

    while not response.endswith(b"\n"):
        chunk = client.recv(1024)

        if not chunk:
            raise RuntimeError("Server closed the connection")

        response += chunk

    return response.decode().strip()


def main():
    with socket.create_connection((HOST, PORT), timeout=5) as client:
        client.settimeout(5)

        requests = [
            (b"PING\n", "PONG"),
            (b"ECHO Hello from the same connection\n",
             "OK Hello from the same connection"),
            (b"PING\n", "PONG")
        ]

        for request, expected in requests:
            client.sendall(request)
            actual = receive_line(client)

            print("Received:", actual)

            if actual != expected:
                raise AssertionError(
                    f"Expected {expected!r}, received {actual!r}"
                )

        print("PASS: Multiple requests handled over one TCP connection.")


if __name__ == "__main__":
    main()
