#!/bin/sh
# TAP test: self-signed HTTPS fetch is blocked without -k.

set -eux

server_port_base=4443
max_port_attempts=5
port_file="${ARTIFACT_LOCAL_DIR}/server.port"
# Use nearby ports so the HTTPS server can start when the default is busy.

# Ensure the helper HTTPS server exits even when the test process gets
# interrupted on CI. The helper uses a short timeout loop and logs to
# curl.log so we can diagnose server teardown issues.
stop_server() {
    server_pid="$1"

    if kill "${server_pid}" 2>/dev/null; then
        wait_limit=10
        waited=0

        while kill -0 "${server_pid}" 2>/dev/null; do
            if [ ${waited} -ge ${wait_limit} ]; then
                echo "Server pid ${server_pid} did not exit; forcing kill." \
                    >&2
                kill -9 "${server_pid}" 2>/dev/null || :
                break
            fi

            "${PYTHON}" -c "import time; time.sleep(0.1)"
            waited=$((waited + 1))
        done

        wait "${server_pid}" 2>/dev/null || :
    fi
}

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
        printf 'ok 1 - self-signed fetch blocked # SKIP python missing\n'
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
    stop_server "${server_pid}"

    printf 'ok 1 - self-signed fetch blocked # SKIP failed to start HTTPS server\n'
    exit 0
fi

verify_output=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "curl-verify")
run_img2sixel "https://localhost:${server_port}/snake.sixel" \
    >"${verify_output}" && command_status=$? || command_status=$?

stop_server "${server_pid}"

# The HTTPS request must fail TLS verification when -k is omitted.
if [ ${command_status} -eq 0 ]; then
    rm -f "${verify_output}"
    printf 'not ok 1 - self-signed fetch unexpectedly succeeded without -k\n'
    exit 1
fi

if [ -s "${verify_output}" ]; then
    rm -f "${verify_output}"
    printf 'not ok 1 - self-signed fetch produced output without -k\n'
    exit 1
fi

rm -f "${verify_output}"
printf 'ok 1 - self-signed fetch blocked without -k\n'
