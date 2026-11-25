
import socket
import time

HOST = "127.0.0.1"
PORT = 9000

def main():
    with socket.create_connection((HOST, PORT), timeout=5) as client:
        client.settimeout(5)

        # Send the first part of the message.
        client.sendall(b"PI")
        time.sleep(0.2)

        # Send the remaining part, including the newline.
        client.sendall(b"NG\n")

        response = b""
        while not response.endswith(b"\n"):
            chunk = client.recv(1024)
            if not chunk:
                raise RuntimeError("Server closed the connection unexpectedly")
            response += chunk

        result = response.decode().strip()
        print("Response:", result)

        if result == "PONG":
            print("PASS: Server handled a fragmented TCP message.")
        else:
            raise AssertionError(f"Expected PONG, received {result!r}")

if __name__ == "__main__":
    main()
