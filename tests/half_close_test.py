import socket

HOST = "127.0.0.1"
PORT = 9000

def main():
    with socket.create_connection((HOST, PORT), timeout=5) as client:
        client.settimeout(5)

        client.sendall(b"PING\n")

        # Close the sending direction, but keep receiving.
        client.shutdown(socket.SHUT_WR)

        response = b""

        while not response.endswith(b"\n"):
            chunk = client.recv(1024)
            if not chunk:
                break
            response += chunk

        result = response.decode().strip()
        print("Response:", result)

        if result == "PONG":
            print("PASS: Server handled TCP half-close correctly.")
        else:
            raise AssertionError(f"Expected PONG, received {result!r}")

if __name__ == "__main__":
    main()
