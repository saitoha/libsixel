#!/bin/sh
# Verify invalid SIXEL_LOG_LINES tokens do not break non-VPTE conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-lines-invalid.six"
log_path="${ARTIFACT_LOCAL_DIR}/timeline-lines-invalid.json"

if run_img2sixel --env SIXEL_LOG_PATH="${log_path}",SIXEL_LOG_LINES=abc \
        --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}"; then
    if [ -s "${output_sixel}" ]; then
        pass 1 "invalid SIXEL_LOG_LINES token keeps conversion stable"
    else
        fail 1 "conversion succeeded but output file is empty"
    fi
else
    fail 1 "conversion with invalid SIXEL_LOG_LINES token failed"
fi

exit 0
