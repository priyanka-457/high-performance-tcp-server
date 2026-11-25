#!/usr/bin/env python3
"""Simple concurrent request benchmark; reports latency percentiles and throughput."""
import argparse, concurrent.futures, socket, statistics, time

p = argparse.ArgumentParser()
p.add_argument("--host", default="127.0.0.1")
p.add_argument("--port", type=int, default=9000)
p.add_argument("--clients", type=int, default=10)
p.add_argument("--requests", type=int, default=1000)
args = p.parse_args()

def worker(start, count):
    samples, failures = [], 0
    for i in range(start, start + count):
        begin = time.perf_counter()
        try:
            with socket.create_connection((args.host, args.port), timeout=5) as s:
                s.sendall(f"PING\n".encode())
                data = b""
                while not data.endswith(b"\n"):
                    part = s.recv(128)
                    if not part: break
                    data += part
                if data != b"PONG\n": raise RuntimeError("unexpected response")
            samples.append((time.perf_counter() - begin) * 1000)
        except Exception:
            failures += 1
    return samples, failures

start = time.perf_counter()
base, extra = divmod(args.requests, args.clients)
with concurrent.futures.ThreadPoolExecutor(max_workers=args.clients) as pool:
    results = list(pool.map(lambda i: worker(i * base + min(i, extra), base + (i < extra)),
                            range(args.clients)))
elapsed = time.perf_counter() - start
latencies = [x for samples, _ in results for x in samples]
failures = sum(f for _, f in results)
latencies.sort()
def pct(q):
    return latencies[min(len(latencies)-1, int((len(latencies)-1)*q))] if latencies else float("nan")
print(f"completed={len(latencies)} failures={failures} elapsed_s={elapsed:.3f}")
print(f"throughput_req_s={len(latencies)/elapsed:.2f}")
if latencies:
    print(f"avg_ms={statistics.mean(latencies):.3f} median_ms={statistics.median(latencies):.3f} "
          f"p95_ms={pct(.95):.3f} p99_ms={pct(.99):.3f}")
