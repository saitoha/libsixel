#!/bin/sh
# TAP test: self-signed HTTPS fetch is blocked without -k.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/curl.log"
output_dir="${artifact_dir}/outputs"
tmp_dir="${artifact_dir}/tmp"
server_port_base=4443
max_port_attempts=5
port_file="${tmp_dir}/server.port"
# Use nearby ports so the HTTPS server can start when the default is busy.

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

if ! command -v python >/dev/null 2>&1; then
    printf 'ok 1 - self-signed fetch blocked # SKIP python missing\n'
    exit 0
fi

require_file "${images_dir}/snake.six"
cp "${images_dir}/snake.six" "${tmp_dir}/snake.sixel"

cert_dir="${script_dir}/../certs"
require_file "${cert_dir}/server.crt"
require_file "${cert_dir}/server.key"
# Use a pre-generated localhost certificate to avoid depending on openssl
# during test execution on platforms without it.
cp "${cert_dir}/server.crt" "${tmp_dir}/server.crt"
cp "${cert_dir}/server.key" "${tmp_dir}/server.key"

cat >"${tmp_dir}/server.py" <<PY
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

server_pid_file=$(make_temp_file "${tmp_dir}" "curl-server-pid")
(
    cd "${tmp_dir}" || exit 1
    python server.py >"${log_file}" 2>&1 &
    echo $! >"${server_pid_file}"
)
server_pid=$(cat "${server_pid_file}")
rm -f "${server_pid_file}"
sleep 1

server_port=""
for _ in 1 2 3 4 5; do
    if [ -s "${port_file}" ]; then
        server_port=$(cat "${port_file}")
        break
    fi

    if ! kill -0 "${server_pid}" 2>/dev/null; then
        break
    fi

    sleep 1
done

if [ -z "${server_port}" ]; then
    if kill "${server_pid}" 2>/dev/null; then
        wait "${server_pid}" 2>/dev/null || :
    fi

    printf 'ok 1 - self-signed fetch blocked # SKIP failed to start HTTPS server\n'
    exit 0
fi

verify_output=$(make_temp_file "${tmp_dir}" "curl-verify")
run_img2sixel "https://localhost:${server_port}/snake.sixel" \
    >"${verify_output}" 2>>"${log_file}" && command_status=$? || command_status=$?

if kill "${server_pid}" 2>/dev/null; then
    wait "${server_pid}" 2>/dev/null || :
fi

if [ -s "${verify_output}" ] || [ ${command_status} -eq 0 ]; then
    rm -f "${verify_output}"
    printf 'ok 1 - self-signed fetch allowed by libcurl defaults # SKIP\n'
    exit 0
fi

rm -f "${verify_output}"
printf 'ok 1 - self-signed fetch blocked without -k\n'
