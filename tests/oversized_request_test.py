import socket

HOST = "127.0.0.1"
PORT = 9000

def main():
    with socket.create_connection((HOST, PORT), timeout=5) as client:
        client.settimeout(5)

        # Send a request line larger than 64 KB.
        client.sendall(b"A" * (65 * 1024) + b"\n")

        response = b""

        while not response.endswith(b"\n"):
            chunk = client.recv(1024)
            if not chunk:
                break
            response += chunk

        result = response.decode().strip()
        print("Response:", result)

        if result == "ERR request too large":
            print("PASS: Server rejected an oversized request.")
        else:
            raise AssertionError(f"Unexpected response: {result!r}")

if __name__ == "__main__":
    main()
