#!/usr/bin/env python3

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("localhost", 8080))
sock.send(b"GET"*5000+b" /hello"*5000+b" HTTP/1.1\r\nHost:localhost:8080\r\n\r\n")
response = sock.recv(4096)
sock.close()
