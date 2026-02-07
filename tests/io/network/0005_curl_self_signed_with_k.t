#!/bin/sh
# TAP test: self-signed HTTPS fetch succeeds when -k is provided.

set -eux

server_port_base=4444
max_port_attempts=5
port_file="${ARTIFACT_LOCAL_DIR}/server.port"
# Use nearby ports so the HTTPS server can start when the default is busy.

script_dir=${test_dir}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

if ! command -v python >/dev/null 2>&1; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON=python3
    else
        printf 'ok 1 - self-signed fetch with -k # SKIP python missing\n'
        exit 0
    fi
else
    PYTHON=python
fi

# Keep a single Python command path for portability across environments
# where only `python3` exists.
export PYTHON


cp "${images_dir}/snake.six" "${ARTIFACT_LOCAL_DIR}/snake.sixel"

cert_dir="${script_dir}/certs"


# Use a pre-generated localhost certificate to avoid depending on openssl
# during test execution on platforms without it.
cp "${cert_dir}/server.crt" "${ARTIFACT_LOCAL_DIR}/server.crt"
cp "${cert_dir}/server.key" "${ARTIFACT_LOCAL_DIR}/server.key"

cat >"${ARTIFACT_LOCAL_DIR}/server.py" <<PY
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
        context.load_cert_chain('server.crt', 'server.key')
        return context.wrap_socket(sock, server_side=True)
    return ssl.wrap_socket(
        sock,
        certfile='server.crt',
        keyfile='server.key',
        server_side=True,
    )

def serve_requests(port):
    with TLSHTTPServer(('localhost', port), SimpleHTTPRequestHandler) as httpd:
        httpd.socket = configure_socket(httpd.socket)
        httpd.timeout = 5
        with open('server.port', 'w', encoding='ascii') as port_fp:
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

server_pid_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "curl-server-pid")
(
    cd "${ARTIFACT_LOCAL_DIR}" || exit 1
    "${PYTHON}" server.py &
    echo $! >"${server_pid_file}"
)
server_pid=$(cat "${server_pid_file}")
rm -f "${server_pid_file}"

server_port=""
for _ in 1 2 3 4 5; do
    if [ -s "${port_file}" ]; then
        server_port=$(cat "${port_file}")
        break
    fi

    if ! kill -0 "${server_pid}" 2>/dev/null; then
        break
    fi

    "${PYTHON}" -c "import time; time.sleep(0.1)"
done

if [ -z "${server_port}" ]; then
    if kill "${server_pid}" 2>/dev/null; then
        wait "${server_pid}" 2>/dev/null || :
    fi

    printf 'ok 1 - self-signed fetch with -k # SKIP failed to start HTTPS server\n'
    exit 0
fi

verify_output="${ARTIFACT_LOCAL_DIR}/https.sixel"
server_ok=1

for attempt in 1 2 3; do
    if run_img2sixel -k "https://localhost:${server_port}/snake.sixel" \
            >"${verify_output}"; then
        server_ok=0
        break
    fi

    "${PYTHON}" -c "import time; time.sleep(0.1)"
done

if kill "${server_pid}" 2>/dev/null; then
    wait "${server_pid}" 2>/dev/null || :
fi

if [ ${server_ok} -ne 0 ] || [ ! -s "${verify_output}" ]; then
    printf 'not ok 1 - self-signed fetch with -k failed\n'
    exit 1
fi

printf 'ok 1 - self-signed fetch succeeds with -k\n'
