#!/bin/sh
# TAP test: self-signed HTTPS fetch succeeds when -k is provided.

set -eux

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || test "${HAVE_LIBFETCH-}" = 1 || {
    printf "1..0 # SKIP network backend support is disabled in this build\n"
    exit 0
}
test "${HAVE_LIBFETCH-}" = 1 && test "${HAVE_EMSCRIPTEN_H-}" = 1 && {
    printf "1..0 # SKIP emscripten fetch backend does not support -k\n"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
set +x
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

server_port_base=4444
max_port_attempts=5
# Use nearby ports so the HTTPS server can start when the default is busy.
port_file="${ARTIFACT_LOCAL_DIR}/server.$$.port"


PYTHON=""
command -v python >/dev/null 2>&1 && PYTHON=python

test -n "${PYTHON}" || {
    command -v python3 >/dev/null 2>&1 && PYTHON=python3
}

test -n "${PYTHON}" || {
    printf 'ok 1 - self-signed fetch with -k # SKIP python missing\n'
    exit 0
}

cert_dir="${TOP_SRCDIR}/tests/data/io/network/certs"
server_script="${TOP_SRCDIR}/tests/data/io/network/https-server.py"
server_root="${TOP_SRCDIR}"

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
    kill -0 "${server_pid}" 2>/dev/null && kill "${server_pid}" 2>/dev/null || :
    wait "${server_pid}" 2>/dev/null || :
    printf 'ok 1 - self-signed fetch with -k # SKIP failed to start HTTPS server\n'
    exit 0
}

server_ok=1

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -k \
    "https://localhost:${server_port}/images/map8.six" >/dev/null && \
    server_ok=0

kill -0 "${server_pid}" 2>/dev/null && kill "${server_pid}" 2>/dev/null || :
wait "${server_pid}" 2>/dev/null || :

test ${server_ok} -eq 0 || {
    echo "not ok" 1 - "self-signed fetch with -k failed"
    exit 0
}

echo "ok" 1 - "self-signed fetch succeeds with -k"
