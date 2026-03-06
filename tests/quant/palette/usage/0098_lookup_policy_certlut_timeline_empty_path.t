#!/bin/sh
# Verify empty SIXEL_LOG_PATH keeps non-FHEDT conversion working.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-empty-path.six"

run_img2sixel --env SIXEL_LOG_PATH= --env SIXEL_LOG_LINES=3 \
    --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}" || {
    echo "not ok" 1 - "conversion failed with empty SIXEL_LOG_PATH"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "output file is empty with empty SIXEL_LOG_PATH"
    exit 0
}

echo "ok" 1 - "empty SIXEL_LOG_PATH keeps conversion successful"
exit 0
