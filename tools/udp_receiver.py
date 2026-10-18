#!/usr/bin/env python3
"""Simple IPv6 UDP receiver for Thread doorbell events."""
import argparse
import socket
from datetime import datetime
p=argparse.ArgumentParser()
p.add_argument('--bind',default='::')
p.add_argument('--port',type=int,default=61631)
a=p.parse_args()
with socket.socket(socket.AF_INET6,socket.SOCK_DGRAM) as sock:
    sock.bind((a.bind,a.port))
    print(f'Listening on [{a.bind}]:{a.port}',flush=True)
    while True:
        msg,addr=sock.recvfrom(1500)
        print(datetime.now().isoformat(timespec='seconds'),addr[0],msg.decode('utf-8','replace'),flush=True)
