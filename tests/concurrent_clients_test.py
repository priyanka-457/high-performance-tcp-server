
import socket
import threading

HOST = "127.0.0.1"
PORT = 9000
CLIENT_COUNT = 500

barrier = threading.Barrier(CLIENT_COUNT)
results = []
errors = []
lock = threading.Lock()


def client_task(client_id):
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as client:
            client.settimeout(5)

            # Wait until all clients are connected.
            barrier.wait(timeout=10)

            client.sendall(b"PING\n")

            response = b""
            while not response.endswith(b"\n"):
                chunk = client.recv(1024)
                if not chunk:
                    raise RuntimeError("Server closed the connection")
                response += chunk

            result = response.decode().strip()

            with lock:
                results.append(result)

            if result != "PONG":
                raise AssertionError(
                    f"Client {client_id}: expected PONG, got {result}"
                )

    except Exception as error:
        with lock:
            errors.append(f"Client {client_id}: {error}")


def main():
    threads = []

    for client_id in range(CLIENT_COUNT):
        thread = threading.Thread(
            target=client_task,
            args=(client_id,)
        )
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

    print("Total clients:", CLIENT_COUNT)
    print("Successful responses:", len(results))
    print("Failed clients:", len(errors))

    for error in errors:
        print("ERROR:", error)

    if len(results) == CLIENT_COUNT and not errors:
        print(f"PASS: All {CLIENT_COUNT} simultaneous clients received PONG.")
    else:
        raise SystemExit("FAIL: Concurrent client test failed.")


if __name__ == "__main__":
    main()
