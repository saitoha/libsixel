#!/bin/sh
# Verify logger open failures do not break non-FHEDT conversion.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-open-failure.six"
invalid_log_path="${ARTIFACT_LOCAL_DIR}/missing-log-dir/timeline.json"

run_img2sixel --env SIXEL_LOG_PATH="${invalid_log_path}" \
    --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}" || {
    echo "not ok" 1 - "conversion failed when logger open returned runtime error"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "conversion succeeded but output file is empty"
    exit 0
}

echo "ok" 1 - "conversion succeeds when logger path is not writable"
exit 0
