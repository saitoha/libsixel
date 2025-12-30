#!/bin/sh
# TAP test: self-signed HTTPS fetch is blocked without -k.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/curl.log"
output_dir="${artifact_dir}/outputs"
tmp_dir="${artifact_dir}/tmp"
server_port=4443

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_feature_available "HAVE_LIBCURL" "curl" "libcurl support"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

if ! command -v openssl >/dev/null 2>&1 || \
        ! command -v python >/dev/null 2>&1; then
    printf 'ok 1 - self-signed fetch blocked # SKIP openssl or python missing\n'
    exit 0
fi

require_file "${images_dir}/snake.six"
cp "${images_dir}/snake.six" "${tmp_dir}/snake.sixel"

(cd "${tmp_dir}" && openssl genrsa >"server.key" 2>>"${log_file}" \
    && openssl req -new -key "server.key" -subj "/CN=localhost" \
        >"server.csr" 2>>"${log_file}" \
    && openssl x509 -req -in "server.csr" -signkey "server.key" \
        -out "server.crt" 2>>"${log_file}" || exit 1)

cat >"${tmp_dir}/server.py" <<PY
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

with TLSHTTPServer(('localhost', ${server_port}), SimpleHTTPRequestHandler) as httpd:
    httpd.socket = configure_socket(httpd.socket)
    for _ in range(1):
        httpd.handle_request()
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

verify_output=$(make_temp_file "${tmp_dir}" "curl-verify")
run_img2sixel "https://localhost:${server_port}/snake.sixel" \
    >"${verify_output}" 2>>"${log_file}" && command_status=$? || command_status=$?

if kill "${server_pid}" 2>/dev/null; then
    wait "${server_pid}" 2>/dev/null || :
fi

if [ -s "${verify_output}" ] || [ ${command_status} -eq 0 ]; then
    printf 'not ok 1 - self-signed fetch should fail without -k\n'
    rm -f "${verify_output}"
    exit 1
fi

rm -f "${verify_output}"
printf 'ok 1 - self-signed fetch blocked without -k\n'
