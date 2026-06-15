#!/usr/bin/env python3
"""M0 hello-bridge client.

Connects to the GTASA-ML plugin's TCP server and prints the heartbeat messages it
streams (one per game frame). This is the Python end of milestone M0 — it proves the
plugin loaded, the per-frame hook fires, and bytes cross the bridge.

Wire format (see protocol/messages.md):
    [4 bytes uint32 little-endian length N][N bytes UTF-8 JSON]

Usage:
    python scripts/m0_heartbeat_client.py            # connect to 127.0.0.1:7777
    python scripts/m0_heartbeat_client.py --host 127.0.0.1 --port 7777

Run the game (with the plugin installed) first, then run this. It will retry the
connection until the plugin's server is up.
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import time


def recv_exactly(sock: socket.socket, n: int) -> bytes:
    """Read exactly n bytes or raise ConnectionError if the peer closes."""
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("plugin closed the connection")
        buf.extend(chunk)
    return bytes(buf)


def read_message(sock: socket.socket) -> dict:
    """Read one length-prefixed JSON message."""
    (length,) = struct.unpack("<I", recv_exactly(sock, 4))
    payload = recv_exactly(sock, length)
    return json.loads(payload.decode("utf-8"))


def connect(host: str, port: int) -> socket.socket:
    """Connect, retrying until the plugin's server accepts."""
    while True:
        try:
            sock = socket.create_connection((host, port), timeout=5)
            sock.settimeout(None)
            print(f"[connected] {host}:{port}")
            return sock
        except OSError:
            print(f"[waiting]  no server at {host}:{port} yet (is the game running?)...")
            time.sleep(1.0)


def main() -> None:
    ap = argparse.ArgumentParser(description="GTASA-ML M0 heartbeat client")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7777)
    args = ap.parse_args()

    sock = connect(args.host, args.port)

    last_frame: int | None = None
    last_report = time.monotonic()
    frames_since_report = 0

    try:
        while True:
            msg = read_message(sock)
            frame = msg.get("frame")
            frames_since_report += 1

            # Report an effective frame rate once a second so we can see the bridge's
            # real throughput (the number that actually matters for RL step rate).
            now = time.monotonic()
            if now - last_report >= 1.0:
                fps = frames_since_report / (now - last_report)
                print(f"frame={frame}  ~{fps:.0f} msg/s  last={msg}")
                last_report = now
                frames_since_report = 0

            if last_frame is not None and frame is not None and frame < last_frame:
                print(f"[warn] frame counter went backwards ({last_frame} -> {frame})")
            last_frame = frame
    except (ConnectionError, OSError) as exc:
        print(f"[disconnected] {exc}")
    except KeyboardInterrupt:
        print("\n[bye]")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
