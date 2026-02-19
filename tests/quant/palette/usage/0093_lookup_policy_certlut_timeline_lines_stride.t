#!/bin/sh
# Verify different SIXEL_LOG_LINES values work without FHEDT.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_stride1="${ARTIFACT_LOCAL_DIR}/timeline-stride1.six"
output_stride3="${ARTIFACT_LOCAL_DIR}/timeline-stride3.six"
log_stride1="${ARTIFACT_LOCAL_DIR}/timeline-stride1.json"
log_stride3="${ARTIFACT_LOCAL_DIR}/timeline-stride3.json"

run_img2sixel --env SIXEL_LOG_PATH="${log_stride1}",SIXEL_LOG_LINES=1 \
    --lookup-policy=certlut -p 4 -o "${output_stride1}" "${small_ppm}" || {
    fail 1 "conversion with SIXEL_LOG_LINES=1 failed"
    exit 0
}

run_img2sixel --env SIXEL_LOG_PATH="${log_stride3}",SIXEL_LOG_LINES=3 \
    --lookup-policy=certlut -p 4 -o "${output_stride3}" "${small_ppm}" || {
    fail 1 "conversion with SIXEL_LOG_LINES=3 failed"
    exit 0
}

test -s "${output_stride1}" || {
    fail 1 "one of the conversion outputs is empty"
    exit 0
}

test -s "${output_stride3}" || {
    fail 1 "one of the conversion outputs is empty"
    exit 0
}

pass 1 "SIXEL_LOG_LINES values are accepted without FHEDT"
exit 0
