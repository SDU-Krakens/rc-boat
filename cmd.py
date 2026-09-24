#!/usr/bin/env python3
"""Send requests to the running Zero program over its Unix socket.

  ./cmd.py cmd gyro cal 500     one request, prints the answer
  ./cmd.py                      prompt, one request per line
"""

import argparse
import socket
import sys

DEFAULT_SOCKET = "/tmp/rc-boat.sock"
PROMPT = "rc-boat> "
QUIT_WORDS = ("exit", "quit")


def request(sock, reader, line):
    sock.sendall((line + "\n").encode())
    answer = reader.readline()
    if not answer:
        return None  # Zero closed the connection
    return answer.rstrip("\n")


def main():
    parser = argparse.ArgumentParser(description="rc-boat command client")
    parser.add_argument("--socket", default=DEFAULT_SOCKET,
                        help="Zero command socket (default %(default)s)")
    parser.add_argument("words", nargs="*", help="request, e.g. set beta 0.1")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        sock.connect(args.socket)
    except OSError as e:
        print(f"error can't connect to {args.socket}: {e.strerror}")
        return 0
    reader = sock.makefile("r", encoding="utf-8", newline="\n")

    if args.words:
        answer = request(sock, reader, " ".join(args.words))
        print(answer if answer is not None else "error connection closed")
        return 0

    while True:
        try:
            line = input(PROMPT).strip()
        except EOFError:
            print()
            break
        if not line:
            continue
        if line in QUIT_WORDS:
            break
        answer = request(sock, reader, line)
        if answer is None:
            print("error connection closed")
            break
        print(answer)
    return 0


if __name__ == "__main__":
    sys.exit(main())
