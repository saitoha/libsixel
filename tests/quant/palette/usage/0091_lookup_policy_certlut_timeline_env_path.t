#!/bin/sh
# Verify setting SIXEL_LOG_PATH does not break non-FHEDT conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-env-path.six"
log_path="${ARTIFACT_LOCAL_DIR}/timeline-env-path.json"

run_img2sixel --env SIXEL_LOG_PATH="${log_path}" --lookup-policy=certlut -p 4 \
    -o "${output_sixel}" "${small_ppm}" || {
    fail 1 "conversion with SIXEL_LOG_PATH failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "conversion succeeded but output file is empty"
    exit 0
}

pass 1 "SIXEL_LOG_PATH works with non-FHEDT conversion"
exit 0
