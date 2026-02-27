#!/bin/sh
# Verify empty SIXEL_LOG_PATH keeps non-FHEDT conversion working.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-empty-path.six"

run_img2sixel --env SIXEL_LOG_PATH= --env SIXEL_LOG_LINES=3 \
    --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}" || {
    fail 1 "conversion failed with empty SIXEL_LOG_PATH"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "output file is empty with empty SIXEL_LOG_PATH"
    exit 0
}

pass 1 "empty SIXEL_LOG_PATH keeps conversion successful"
exit 0
