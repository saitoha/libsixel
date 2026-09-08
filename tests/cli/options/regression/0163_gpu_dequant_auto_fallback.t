#!/bin/sh
# Verify that unavailable automatic GPU dequantization falls back to the CPU.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/map8.six"
auto_output="${ARTIFACT_LOCAL_DIR}/0163-gpu-auto-fallback-$$.png"
cpu_output="${ARTIFACT_LOCAL_DIR}/0163-gpu-cpu-control-$$.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -d lso_undither:Vlight \
    -G auto:D4294967295 <"${input_image}" >"${auto_output}" || {
    echo "not ok 1 - unavailable automatic GPU conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -d lso_undither:Vlight \
    -G off <"${input_image}" >"${cpu_output}" || {
    echo "not ok 1 - CPU control conversion failed"
    exit 0
}

cmp -s "${auto_output}" "${cpu_output}" || {
    echo "not ok 1 - automatic GPU fallback differs from CPU output"
    exit 0
}

echo "ok 1 - unavailable automatic GPU dequantization falls back to CPU"
exit 0
