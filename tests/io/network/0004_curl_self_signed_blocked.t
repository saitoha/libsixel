#!/bin/sh
# TAP test: self-signed HTTPS fetch is blocked without -k.

set -eux

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || test "${HAVE_LIBFETCH-}" = 1 || {
    printf "1..0 # SKIP network backend support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

server_port_base=4443
max_port_attempts=5
# Use nearby ports so the HTTPS server can start when the default is busy.
port_file="${ARTIFACT_LOCAL_DIR}/server.$$.port"


PYTHON=""
command -v python >/dev/null 2>&1 && PYTHON=python

test -n "${PYTHON}" || {
    command -v python3 >/dev/null 2>&1 && PYTHON=python3
}

test -n "${PYTHON}" || {
    printf 'ok 1 - self-signed fetch blocked # SKIP python missing\n'
    exit 0
}

cert_dir="${TOP_SRCDIR}/tests/data/io/network/certs"
server_script="${TOP_SRCDIR}/tests/data/io/network/https-server.py"
server_root="${TOP_SRCDIR}"

# Keep this server as a direct child.  PID-file cleanup is fragile on
# Git Bash/MSYS when Meson runs TAP tests in parallel, because a stale
# PID can target an unrelated worker.
(
    cd "${server_root}" || exit 1
    exec "${PYTHON}" "${server_script}" \
        --host localhost \
        --port-base "${server_port_base}" \
        --max-port-attempts "${max_port_attempts}" \
        --port-file "${port_file}" \
        --cert-file "${cert_dir}/server.crt" \
        --key-file "${cert_dir}/server.key" \
        --requests 1
) &
server_pid=$!

server_port=""
while test ! -s "${port_file}"; do
    kill -0 "${server_pid}" 2>/dev/null || break
done
test -s "${port_file}" && {
    IFS= read -r server_port < "${port_file}" || test -n "${server_port}"
}

test -n "${server_port}" || {
    kill "${server_pid}" 2>/dev/null || :
    wait "${server_pid}" 2>/dev/null || :
    printf 'ok 1 - self-signed fetch blocked # SKIP failed to start HTTPS server\n'
    exit 0
}

verify_output="${ARTIFACT_LOCAL_DIR}/curl-verify"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "https://localhost:${server_port}/images/map8.six" \
    >"${verify_output}" && command_status=0 || command_status=$?

kill "${server_pid}" 2>/dev/null || :
wait "${server_pid}" 2>/dev/null || :

# The HTTPS request must fail TLS verification when -k is omitted.
test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "self-signed fetch unexpectedly succeeded without -k"
    exit 0
}

test ! -s "${verify_output}" || {
    echo "not ok" 1 - "self-signed fetch produced output without -k"
    exit 0
}

echo "ok" 1 - "self-signed fetch blocked without -k"
