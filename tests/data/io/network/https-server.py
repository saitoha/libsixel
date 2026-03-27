#!/usr/bin/env python3
"""Start a minimal HTTPS server for TAP network tests."""

import argparse
import io
import ssl
import sys

try:
    from http.server import SimpleHTTPRequestHandler
    from socketserver import TCPServer
except ImportError:
    from SimpleHTTPServer import SimpleHTTPRequestHandler  # type: ignore
    from SocketServer import TCPServer  # type: ignore


class TLSHTTPServer(TCPServer):
    allow_reuse_address = True


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port-base", required=True, type=int)
    parser.add_argument("--max-port-attempts", default=5, type=int)
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--cert-file", required=True)
    parser.add_argument("--key-file", required=True)
    parser.add_argument("--requests", default=5, type=int)
    return parser.parse_args()


def write_port_file(path, port):
    with io.open(path, "w", encoding="ascii") as port_fp:
        port_fp.write(str(port))


def configure_socket(sock, cert_file, key_file):
    if hasattr(ssl, "SSLContext"):
        protocol = getattr(ssl, "PROTOCOL_TLS_SERVER", ssl.PROTOCOL_TLS)
        context = ssl.SSLContext(protocol)
        context.load_cert_chain(cert_file, key_file)
        return context.wrap_socket(sock, server_side=True)

    return ssl.wrap_socket(
        sock,
        certfile=cert_file,
        keyfile=key_file,
        server_side=True,
    )


def serve_requests(host, port, port_file, cert_file, key_file, request_count):
    httpd = TLSHTTPServer((host, port), SimpleHTTPRequestHandler)
    try:
        httpd.socket = configure_socket(httpd.socket, cert_file, key_file)
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
            serve_requests(
                args.host,
                port,
                args.port_file,
                args.cert_file,
                args.key_file,
                args.requests,
            )
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
