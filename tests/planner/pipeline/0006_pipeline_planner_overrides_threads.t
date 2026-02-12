#!/bin/sh
# TAP test: override thread split is emitted.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

pipeline_log=$(SIXEL_DITHER_PARALLEL_THREADS_MAX=1 SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 SIXEL_THREADS=6 run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" 2>&1) || {
    fail 1 "override thread split run failed"
    exit 0
}
printf '%s' "${pipeline_log}" >&2

threads_line=$(printf '%s' "${pipeline_log}" | grep "band_height=" | head -n 1) || threads_line=""
printf '%s' "${threads_line}" | grep -q '^[[:space:]]*band_height=' || {
    fail 1 "override thread split"
    exit 0
}

pass 1 "override thread split"

exit 0
