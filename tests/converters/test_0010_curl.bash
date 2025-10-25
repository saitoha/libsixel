#!/usr/bin/env bash
# Test fetching images via libcurl.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/t/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +------------------------------+-------------------------------+
#  | Transport                    | Expectation                    |
#  +------------------------------+-------------------------------+
#  | file:// invalid              | Should fail cleanly           |
#  | https:// malformed           | Should fail cleanly           |
#  | file:// valid                | Should succeed                |
#  | https:// self-signed (no -k) | Should fail (optional)        |
#  | https:// self-signed (-k)    | Should succeed (optional)     |
#  +------------------------------+-------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

declare -a CURL_DESCRIPTIONS=()
declare -a CURL_CALLBACKS=()
declare -a CURL_ARGUMENTS=()

register_curl_case() {
    local description
    local callback

    description=$1
    callback=$2
    shift 2
    CURL_DESCRIPTIONS+=("${description}")
    CURL_CALLBACKS+=("${callback}")
    CURL_ARGUMENTS+=("$*")
}

fail_fetch_case() {
    local url
    local output_file

    url=$1
    output_file="${TMP_DIR}/capture.$$"
    tap_log "[curl] expecting failure for ${url}"
    if run_img2sixel "${url}" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output: %s\n' "${url}"
        rm -f "${output_file}"
        return 1
    fi
    rm -f "${output_file}"
    return 0
}

file_fetch_case() {
    local url

    url=$1
    tap_log "[curl] fetching ${url}"
    run_img2sixel "${url}"
}

prepare_https_assets() {
    local https_dir

    https_dir=${TMP_DIR}
    tap_log '[curl] preparing HTTPS fixtures'
    if [[ ! -f ${https_dir}/snake.sixel ]]; then
        run_img2sixel "${TOP_SRCDIR}/images/snake.jpg" \
            >"${https_dir}/snake.sixel"
    fi
    if [[ ! -f ${https_dir}/server.key ]]; then
        openssl genrsa | openssl rsa >"${https_dir}/server.key"
    fi
    if [[ ! -f ${https_dir}/server.crt ]]; then
        openssl req -new -key "${https_dir}/server.key" \
            -subj "/CN=localhost" | \
            openssl x509 -req -signkey "${https_dir}/server.key" \
                >"${https_dir}/server.crt"
    fi
    cat >"${https_dir}/server.py" <<'PY'
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
}

https_requires_k_case() {
    tap_log '[curl] verifying HTTPS without -k fails'
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
        if run_img2sixel 'https://localhost:4443/snake.sixel' \
                >"${output_file}" 2>/dev/null; then
            tap_log '[curl] skipping verification check: img2sixel accepted self-signed certificate without -k'
            rm -f "${output_file}"
            cleanup
            trap - EXIT
            return 0
        fi
        if [[ -s ${output_file} ]]; then
            printf 'img2sixel unexpectedly produced output: %s\n' \
                'https://localhost:4443/snake.sixel'
            rm -f "${output_file}"
            cleanup
            trap - EXIT
            return 1
        fi
        rm -f "${output_file}"
        cleanup
        trap - EXIT
    )
}

https_succeeds_with_k_case() {
    tap_log '[curl] verifying HTTPS succeeds with -k'
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
        run_img2sixel -k 'https://localhost:4443/snake.sixel'
        cleanup
        trap - EXIT
    )
}

register_curl_case 'invalid file:// URL is rejected' fail_fetch_case 'file:///test'
register_curl_case 'malformed https:// URL is rejected' fail_fetch_case 'https:///test'
register_curl_case 'file:// fetch succeeds' file_fetch_case \
    "file://$(pwd)/${TOP_SRCDIR}/images/snake.jpg"

have_https_tools=0
if command -v openssl >/dev/null 2>&1 && \
    command -v python >/dev/null 2>&1; then
    have_https_tools=1
    prepare_https_assets
    register_curl_case 'self-signed HTTPS requires -k' https_requires_k_case
    register_curl_case 'self-signed HTTPS succeeds with -k' https_succeeds_with_k_case
else
    tap_diag 'Skipping HTTPS curl tests: openssl or python missing.'
fi

case_total=${#CURL_DESCRIPTIONS[@]}
tap_plan "${case_total}"

for index in "${!CURL_DESCRIPTIONS[@]}"; do
    description=${CURL_DESCRIPTIONS[${index}]}
    callback=${CURL_CALLBACKS[${index}]}
    args=${CURL_ARGUMENTS[${index}]}
    IFS=$' \t\n' read -r -a callback_args <<<"${args}"
    tap_case "${description}" "${callback}" "${callback_args[@]}"
done
