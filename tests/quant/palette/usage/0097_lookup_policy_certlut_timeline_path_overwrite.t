#!/bin/sh
# Verify pre-existing log files do not break non-FHEDT conversion.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_a="${ARTIFACT_LOCAL_DIR}/timeline-overwrite-a.six"
output_b="${ARTIFACT_LOCAL_DIR}/timeline-overwrite-b.six"
log_path="${ARTIFACT_LOCAL_DIR}/timeline-overwrite.json"

printf '%s\n' 'sentinel-before-run' >"${log_path}"

run_img2sixel --env SIXEL_LOG_PATH="${log_path}" --lookup-policy=certlut -p 4 \
    -o "${output_a}" "${small_ppm}" || {
    echo "not ok" 1 - "first conversion run failed"
    exit 0
}

run_img2sixel --env SIXEL_LOG_PATH="${log_path}" --lookup-policy=certlut -p 4 \
    -o "${output_b}" "${small_ppm}" || {
    echo "not ok" 1 - "second conversion run failed"
    exit 0
}

test -s "${output_a}" || {
    echo "not ok" 1 - "one of the conversion outputs is empty"
    exit 0
}

test -s "${output_b}" || {
    echo "not ok" 1 - "one of the conversion outputs is empty"
    exit 0
}

echo "ok" 1 - "pre-existing log file path is accepted"
exit 0
