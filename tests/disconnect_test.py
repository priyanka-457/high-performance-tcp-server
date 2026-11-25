
import socket

HOST = "127.0.0.1"
PORT = 9000


def send_ping():
    with socket.create_connection((HOST, PORT), timeout=5) as client:
        client.settimeout(5)
        client.sendall(b"PING\n")

        response = b""
        while not response.endswith(b"\n"):
            chunk = client.recv(1024)
            if not chunk:
                raise RuntimeError("Server closed connection unexpectedly")
            response += chunk

        return response.decode().strip()


def main():
    # First client connects, communicates, and disconnects.
    first_response = send_ping()
    print("First client:", first_response)

    if first_response != "PONG":
        raise AssertionError("First client failed")

    print("First client disconnected.")

    # A new client connects after the first one disconnects.
    second_response = send_ping()
    print("Second client:", second_response)

    if second_response != "PONG":
        raise AssertionError("Second client failed")

    print("PASS: Server accepted a new client after disconnection.")


if __name__ == "__main__":
    main()
