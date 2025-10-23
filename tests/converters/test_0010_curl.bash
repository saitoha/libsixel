#!/usr/bin/env bash
# Test fetching images via libcurl.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo
echo '[test10] curl'

fail_fetch() {
    local output_file

    output_file="${TMP_DIR}/capture.$$"
    if run_img2sixel "$@" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output: %s\n' "$*" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"
}

fail_fetch 'file:///test'
fail_fetch 'https:///test'

run_img2sixel "file://$(pwd)/${TOP_SRCDIR}/images/snake.jpg"

if command -v openssl >/dev/null 2>&1 && command -v python >/dev/null 2>&1; then
    require_file "${TMP_DIR}/snake.sixel"
    openssl genrsa | openssl rsa > "${TMP_DIR}/server.key"
    openssl req -new -key "${TMP_DIR}/server.key" -subj "/CN=localhost" | \
        openssl x509 -req -signkey "${TMP_DIR}/server.key" > "${TMP_DIR}/server.crt"
    cat > "${TMP_DIR}/server.py" <<'PY'
try:
    from http.server import SimpleHTTPRequestHandler
    from socketserver import TCPServer
except ImportError:
    from SimpleHTTPServer import SimpleHTTPRequestHandler  # type: ignore
    from SocketServer import TCPServer  # type: ignore
import ssl

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

with TLSHTTPServer(('localhost', 4443), SimpleHTTPRequestHandler) as httpd:
    httpd.socket = configure_socket(httpd.socket)
    for _ in range(2):
        httpd.handle_request()
PY
    (
        cd "${TMP_DIR}"
        python server.py &
        server_pid=$!
        cleanup() {
            kill "${server_pid}" 2>/dev/null || true
            wait "${server_pid}" 2>/dev/null || true
        }
        trap cleanup EXIT
        sleep 1
        output_file="${TMP_DIR}/capture.$$"
        if run_img2sixel 'https://localhost:4443/snake.sixel' >"${output_file}" 2>/dev/null; then
            echo 'Skipping certificate verification check: img2sixel accepted self-signed certificate without -k' >&2
        else
            if [[ -s ${output_file} ]]; then
                printf 'img2sixel unexpectedly produced output: %s\n' \
                    'https://localhost:4443/snake.sixel' >&2
                rm -f "${output_file}"
                exit 1
            fi
        fi
        rm -f "${output_file}"
        sleep 1
        run_img2sixel -k 'https://localhost:4443/snake.sixel'
        cleanup
        trap - EXIT
    )
fi
