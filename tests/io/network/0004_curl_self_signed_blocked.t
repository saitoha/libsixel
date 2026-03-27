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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

server_port_base=4443
max_port_attempts=5
# Use nearby ports so the HTTPS server can start when the default is busy.
port_file="${ARTIFACT_LOCAL_DIR}/server.port"


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

server_pid_file="${ARTIFACT_LOCAL_DIR}/curl-server-pid"
(
    cd "${server_root}" || exit 1
    "${PYTHON}" "${server_script}" \
        --host localhost \
        --port-base "${server_port_base}" \
        --max-port-attempts "${max_port_attempts}" \
        --port-file "${port_file}" \
        --cert-file "${cert_dir}/server.crt" \
        --key-file "${cert_dir}/server.key" \
        --requests 5 &
    echo $! >"${server_pid_file}"
)
server_pid=$(cat "${server_pid_file}")

server_port=""
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    test -s "${port_file}" && {
        server_port=$(cat "${port_file}")
        break
    }

    "${PYTHON}" -c "import time; time.sleep(0.1)"
done

test -n "${server_port}" || {
wait_limit=10
waited=0
kill "${server_pid}" 2>/dev/null || :
while kill -0 "${server_pid}" 2>/dev/null; do
    test ${waited} -lt ${wait_limit} || {
        echo "Server pid ${server_pid} did not exit; forcing kill." >&2
        kill -9 "${server_pid}" 2>/dev/null || :
        break
    }

    "${PYTHON}" -c "import time; time.sleep(0.01)"
    waited=$((waited + 1))
done
wait "${server_pid}" 2>/dev/null || :
    printf 'ok 1 - self-signed fetch blocked # SKIP failed to start HTTPS server\n'
    exit 0
}

verify_output="${ARTIFACT_LOCAL_DIR}/curl-verify"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "https://localhost:${server_port}/images/map8.six" \
    >"${verify_output}" && command_status=0 || command_status=$?

wait_limit=10
waited=0
kill "${server_pid}" 2>/dev/null || :
while kill -0 "${server_pid}" 2>/dev/null; do
    test ${waited} -lt ${wait_limit} || {
        echo "Server pid ${server_pid} did not exit; forcing kill." >&2
        kill -9 "${server_pid}" 2>/dev/null || :
        break
    }

    "${PYTHON}" -c "import time; time.sleep(0.01)"
    waited=$((waited + 1))
done
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
