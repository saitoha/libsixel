#!/bin/sh
# TAP test: self-signed HTTPS fetch succeeds when -k is provided.

set -eux

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || {
    printf "1..0 # SKIP libcurl or WinHTTP support is disabled in this build"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

server_port_base=4444
max_port_attempts=5
# Use nearby ports so the HTTPS server can start when the default is busy.
port_file="${ARTIFACT_LOCAL_DIR}/server.port"
script_dir=${0%[/\\]*}

echo "1..1"
set -v

PYTHON=""
command -v python >/dev/null 2>&1 && PYTHON=python

test -n "${PYTHON}" || {
    command -v python3 >/dev/null 2>&1 && PYTHON=python3
}

test -n "${PYTHON}" || {
    printf 'ok 1 - self-signed fetch with -k # SKIP python missing\n'
    exit 0
}

cert_dir="${script_dir}/certs"
server_root="${TOP_SRCDIR}"

cat >"${ARTIFACT_LOCAL_DIR}/server.py" <<PY &
try:
    from http.server import SimpleHTTPRequestHandler
    from socketserver import TCPServer
except ImportError:
    from SimpleHTTPServer import SimpleHTTPRequestHandler  # type: ignore
    from SocketServer import TCPServer  # type: ignore
import ssl
import sys

class TLSHTTPServer(TCPServer):
    allow_reuse_address = True


def configure_socket(sock):
    if hasattr(ssl, 'SSLContext'):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain('${cert_dir}/server.crt',
                                '${cert_dir}/server.key')
        return context.wrap_socket(sock, server_side=True)
    return ssl.wrap_socket(
        sock,
        certfile='${cert_dir}/server.crt',
        keyfile='${cert_dir}/server.key',
        server_side=True,
    )

def serve_requests(port):
    with TLSHTTPServer(('localhost', port), SimpleHTTPRequestHandler) as httpd:
        httpd.socket = configure_socket(httpd.socket)
        httpd.timeout = 5
        with open('${port_file}', 'w', encoding='ascii') as port_fp:
            port_fp.write(str(port))

        for _ in range(5):
            httpd.handle_request()


def main():
    last_error = None

    for offset in range(${max_port_attempts}):
        port = ${server_port_base} + offset
        try:
            serve_requests(port)
            return 0
        except OSError as exc:
            last_error = exc
            continue

    sys.stderr.write(
        f"Failed to bind after ${max_port_attempts} attempts: {last_error}\n",
    )
    return 75


if __name__ == '__main__':
    sys.exit(main())
PY

server_pid_file="${ARTIFACT_LOCAL_DIR}/curl-server-pid"
cd "${server_root}" && "${PYTHON}" "${ARTIFACT_LOCAL_DIR}/server.py" &
echo $! >"${server_pid_file}"
server_pid=$(cat "${server_pid_file}")

server_port=""
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    test -s "${port_file}" && {
        server_port=$(cat "${port_file}")
        break
    }

    kill -0 "${server_pid}" 2>/dev/null || break

    "${PYTHON}" -c "import time; time.sleep(0.01)"
done

test -n "${server_port}" || {
    kill "${server_pid}" 2>/dev/null || :
    wait "${server_pid}" 2>/dev/null || :
    printf 'ok 1 - self-signed fetch with -k # SKIP failed to start HTTPS server\n'
    exit 0
}

verify_output="${ARTIFACT_LOCAL_DIR}/https.sixel"
server_ok=1

for _ in 1 2 3; do
    run_img2sixel -k "https://localhost:${server_port}/images/map8.six" \
        >"${verify_output}" && {
        server_ok=0
        break
    }

    "${PYTHON}" -c "import time; time.sleep(0.01)"
done

kill "${server_pid}" 2>/dev/null || :
wait "${server_pid}" 2>/dev/null || :

test ${server_ok} -eq 0 || {
    fail 1 "self-signed fetch with -k failed"
    exit 0
}

test -s "${verify_output}" || {
    fail 1 "self-signed fetch with -k failed"
    exit 0
}

pass 1 "self-signed fetch succeeds with -k"
