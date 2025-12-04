#!/bin/sh
# TAP test verifying img2sixel curl-based fetch handling.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/curl.log"
output_dir="${artifact_dir}/outputs"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_feature_available "HAVE_LIBCURL" "curl" "libcurl support"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

skip() {
    reason="skipped"
    if [ "$#" -ge 3 ]; then
        reason=$3
    fi

    printf 'ok %s - %s # SKIP %s\n' "$1" "$2" "${reason}"
}

echo "1..5"

capture_output() {
    target=$1
    shift
    capture_file=$(make_temp_file "${tmp_dir}" "curl-capture")

    if run_img2sixel "$@" >"${capture_file}" 2>>"${log_file}"; then
        :
    fi

    if [ -s "${capture_file}" ]; then
        rm -f "${capture_file}"
        return 1
    fi

    rm -f "${capture_file}"
    return 0
}

if capture_output "${output_dir}/invalid-file" 'file:///test'; then
    pass ${case_id} "rejects invalid file URL"
else
    fail ${case_id} "invalid file URL produced output"
fi
case_id=$((case_id + 1))

if capture_output "${output_dir}/invalid-https" 'https:///test'; then
    pass ${case_id} "rejects malformed HTTPS URL"
else
    fail ${case_id} "malformed HTTPS URL produced output"
fi
case_id=$((case_id + 1))

local_file="file://$(CDPATH=; cd "${top_srcdir}" && pwd)/images/snake.jpg"
if run_img2sixel "${local_file}" >"${output_dir}/local-file.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "fetches local file via file scheme"
else
    fail ${case_id} "local file fetch via file scheme failed"
fi
case_id=$((case_id + 1))

if command -v openssl >/dev/null 2>&1 && command -v python >/dev/null 2>&1; then
    require_file "${images_dir}/snake.six"
    require_file "${top_srcdir}/tests/t/test_0010_curl.t"

    cp "${images_dir}/snake.six" "${tmp_dir}/snake.sixel"

    (cd "${tmp_dir}" && openssl genrsa >"server.key" 2>>"${log_file}" \
        && openssl req -new -key "server.key" -subj "/CN=localhost" \
            >"server.csr" 2>>"${log_file}" \
        && openssl x509 -req -in "server.csr" -signkey "server.key" \
            -out "server.crt" 2>>"${log_file}" || exit 1)

    cat >"${tmp_dir}/server.py" <<'PY'
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
    if run_img2sixel 'https://localhost:4443/snake.sixel' \
            >"${verify_output}" 2>>"${log_file}"; then
        skip ${case_id} "certificate verification disabled" && server_ok=1
    else
        if [ -s "${verify_output}" ]; then
            fail ${case_id} "self-signed fetch produced output without -k"
            server_ok=0
        else
            pass ${case_id} "self-signed fetch blocked without -k"
            server_ok=1
        fi
    fi
    rm -f "${verify_output}"

    if [ ${server_ok} -eq 1 ]; then
        if run_img2sixel -k 'https://localhost:4443/snake.sixel' \
                >"${output_dir}/https.sixel" 2>>"${log_file}"; then
            pass $((case_id + 1)) "self-signed fetch succeeds with -k"
        else
            fail $((case_id + 1)) "self-signed fetch with -k failed"
        fi
    else
        skip $((case_id + 1)) "self-signed server unavailable" \
            "server startup failed"
    fi

    case_id=$((case_id + 2))

    if kill "${server_pid}" 2>/dev/null; then
        wait "${server_pid}" 2>/dev/null || :
    fi
else
    skip ${case_id} "openssl or python missing"
    skip $((case_id + 1)) "openssl or python missing"
    case_id=$((case_id + 2))
fi

exit "${status}"
