#!/bin/sh
# Verify SIXEL_LOG_LINES=0 does not break non-VPTE conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/timeline-lines-zero.six"
log_path="${ARTIFACT_LOCAL_DIR}/timeline-lines-zero.json"

if run_img2sixel --env SIXEL_LOG_PATH="${log_path}",SIXEL_LOG_LINES=0 \
        --lookup-policy=certlut -p 4 -o "${output_sixel}" "${small_ppm}"; then
    if [ -s "${output_sixel}" ]; then
        pass 1 "SIXEL_LOG_LINES=0 keeps non-VPTE conversion stable"
    else
        fail 1 "conversion succeeded but output file is empty"
    fi
else
    fail 1 "conversion with SIXEL_LOG_LINES=0 failed"
fi

exit 0
