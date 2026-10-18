#!/usr/bin/env python3
"""UDP doorbell receiver supporting IPv4 and IPv6 simultaneously."""
import argparse
import selectors
import socket
from datetime import datetime

parser = argparse.ArgumentParser()
parser.add_argument("--bind", default="0.0.0.0", help="IPv4 bind address (default all)")
parser.add_argument("--bind6", default="::", help="IPv6 bind address (default all)")
parser.add_argument("--port", type=int, default=61631)
args = parser.parse_args()

if not 1 <= args.port <= 65535:
    parser.error("port must be 1..65535")

selector = selectors.DefaultSelector()
sockets = []
for family, address, label in (
    (socket.AF_INET, args.bind, "IPv4"),
    (socket.AF_INET6, args.bind6, "IPv6"),
):
    try:
        sock = socket.socket(family, socket.SOCK_DGRAM)
        if family == socket.AF_INET6:
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
        sock.bind((address, args.port))
        sock.setblocking(False)
        selector.register(sock, selectors.EVENT_READ)
        sockets.append(sock)
        print(f"Listening {label} {address}:{args.port}", flush=True)
    except OSError as exc:
        print(f"{label} unavailable: {exc}", flush=True)

if not sockets:
    raise SystemExit("Failed to bind IPv4 or IPv6 UDP socket")
try:
    while True:
        for key, _ in selector.select():
            msg, addr = key.fileobj.recvfrom(1500)
            print(datetime.now().isoformat(timespec="seconds"),
                  addr[0], msg.decode("utf-8", "replace"), flush=True)
except KeyboardInterrupt:
    pass
finally:
    selector.close()
    for sock in sockets:
        sock.close()
