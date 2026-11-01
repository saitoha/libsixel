#!/bin/sh
# Verify different SIXEL_LOG_LINES values work without FHEDT.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_stride1="${ARTIFACT_LOCAL_DIR}/timeline-stride1.six"
output_stride3="${ARTIFACT_LOCAL_DIR}/timeline-stride3.six"
log_stride1="${ARTIFACT_LOCAL_DIR}/timeline-stride1.json"
log_stride3="${ARTIFACT_LOCAL_DIR}/timeline-stride3.json"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOG_PATH="${log_stride1}" --env SIXEL_LOG_LINES=1 \
    --lookup-policy=certlut -p 4 -o "${output_stride1}" "${small_ppm}" || {
    echo "not ok" 1 - "conversion with SIXEL_LOG_LINES=1 failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOG_PATH="${log_stride3}" --env SIXEL_LOG_LINES=3 \
    --lookup-policy=certlut -p 4 -o "${output_stride3}" "${small_ppm}" || {
    echo "not ok" 1 - "conversion with SIXEL_LOG_LINES=3 failed"
    exit 0
}

test -s "${output_stride1}" || {
    echo "not ok" 1 - "one of the conversion outputs is empty"
    exit 0
}

test -s "${output_stride3}" || {
    echo "not ok" 1 - "one of the conversion outputs is empty"
    exit 0
}

echo "ok" 1 - "SIXEL_LOG_LINES values are accepted without FHEDT"
exit 0
