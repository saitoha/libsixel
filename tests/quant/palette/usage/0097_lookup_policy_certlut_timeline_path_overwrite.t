#!/bin/sh
# Verify pre-existing log files do not break non-VPTE conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_a="${ARTIFACT_LOCAL_DIR}/timeline-overwrite-a.six"
output_b="${ARTIFACT_LOCAL_DIR}/timeline-overwrite-b.six"
log_path="${ARTIFACT_LOCAL_DIR}/timeline-overwrite.json"

printf '%s\n' 'sentinel-before-run' > "${log_path}"

if ! run_img2sixel --env SIXEL_LOG_PATH="${log_path}" --lookup-policy=certlut -p 4 \
        -o "${output_a}" "${small_ppm}"; then
    fail 1 "first conversion run failed"
    exit 0
fi

if ! run_img2sixel --env SIXEL_LOG_PATH="${log_path}" --lookup-policy=certlut -p 4 \
        -o "${output_b}" "${small_ppm}"; then
    fail 1 "second conversion run failed"
    exit 0
fi

if [ -s "${output_a}" ] && [ -s "${output_b}" ]; then
    pass 1 "pre-existing log file path is accepted"
else
    fail 1 "one of the conversion outputs is empty"
fi

exit 0
