#!/usr/bin/env python3
"""Start a minimal HTTP server for TAP network tests."""

import argparse
import io
import sys

try:
    from http.server import SimpleHTTPRequestHandler
    from socketserver import TCPServer
except ImportError:
    from SimpleHTTPServer import SimpleHTTPRequestHandler  # type: ignore
    from SocketServer import TCPServer  # type: ignore


class HTTPServer(TCPServer):
    allow_reuse_address = True


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port-base", required=True, type=int)
    parser.add_argument("--max-port-attempts", default=5, type=int)
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--requests", default=5, type=int)
    return parser.parse_args()


def write_port_file(path, port):
    with io.open(path, "w", encoding="ascii") as port_fp:
        port_fp.write(str(port))


def serve_requests(host, port, port_file, request_count):
    httpd = HTTPServer((host, port), SimpleHTTPRequestHandler)
    try:
        httpd.timeout = 5
        write_port_file(port_file, port)

        for _ in range(request_count):
            httpd.handle_request()
    finally:
        httpd.server_close()


def main():
    args = parse_args()
    last_error = None

    for offset in range(args.max_port_attempts):
        port = args.port_base + offset
        try:
            serve_requests(args.host, port, args.port_file, args.requests)
            return 0
        except OSError as exc:
            last_error = exc

    sys.stderr.write(
        "Failed to bind after {0} attempts: {1}\n".format(
            args.max_port_attempts,
            last_error,
        )
    )
    return 75


if __name__ == "__main__":
    raise SystemExit(main())
