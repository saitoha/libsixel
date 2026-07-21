#!/bin/sh
# TAP test: forced GPU rejection closes an already-started SIXEL DCS.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
out_file="${ARTIFACT_LOCAL_DIR}/pipeline-gpu-force-reject.six"
probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
expected="2638293190 15"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
        --env SIXEL_GPU_POLICY=force \
        "${probe_image}" >"${out_file}" 2>/dev/null
rc=$?
set -e

test "${rc}" -ne 0 || {
    echo "not ok" 1 - "forced GPU unsupported request unexpectedly passed"
    exit 0
}

checksum=$(cksum <"${out_file}") || {
    echo "not ok" 1 - "forced GPU rejection DCS checksum failed"
    exit 0
}

test "${checksum}" = "${expected}" || {
    echo "not ok" 1 - "forced GPU rejection closed DCS"
    exit 0
}

echo "ok" 1 - "forced GPU rejection closed DCS"
exit 0
