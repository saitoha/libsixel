#!/bin/sh
# Verify different SIXEL_LOG_LINES values work without VPTE.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

small_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output_stride1="${ARTIFACT_LOCAL_DIR}/timeline-stride1.six"
output_stride3="${ARTIFACT_LOCAL_DIR}/timeline-stride3.six"
log_stride1="${ARTIFACT_LOCAL_DIR}/timeline-stride1.json"
log_stride3="${ARTIFACT_LOCAL_DIR}/timeline-stride3.json"

if ! run_img2sixel --env SIXEL_LOG_PATH="${log_stride1}",SIXEL_LOG_LINES=1 \
        --lookup-policy=certlut -p 4 -o "${output_stride1}" "${small_ppm}"; then
    fail 1 "conversion with SIXEL_LOG_LINES=1 failed"
    exit 0
fi
if ! run_img2sixel --env SIXEL_LOG_PATH="${log_stride3}",SIXEL_LOG_LINES=3 \
        --lookup-policy=certlut -p 4 -o "${output_stride3}" "${small_ppm}"; then
    fail 1 "conversion with SIXEL_LOG_LINES=3 failed"
    exit 0
fi

if [ -s "${output_stride1}" ] && [ -s "${output_stride3}" ]; then
    pass 1 "SIXEL_LOG_LINES values are accepted without VPTE"
else
    fail 1 "one of the non-VPTE outputs is empty"
fi

exit 0
