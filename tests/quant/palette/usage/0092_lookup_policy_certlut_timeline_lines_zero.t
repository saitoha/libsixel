#!/bin/sh
# Verify SIXEL_LOG_LINES=0 does not break non-FHEDT conversion.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-lines-zero.six"
log_path="${ARTIFACT_LOCAL_DIR}/timeline-lines-zero.json"

run_img2sixel --env SIXEL_LOG_PATH="${log_path}" --env SIXEL_LOG_LINES=0 \
    --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}" || {
    fail 1 "conversion with SIXEL_LOG_LINES=0 failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "conversion succeeded but output file is empty"
    exit 0
}

pass 1 "SIXEL_LOG_LINES=0 keeps non-FHEDT conversion stable"
exit 0
