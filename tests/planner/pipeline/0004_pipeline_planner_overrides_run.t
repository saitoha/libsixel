#!/bin/sh
# TAP test: pipeline planner runs with override environment variables.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

SIXEL_DITHER_PARALLEL_THREADS_MAX=1         SIXEL_DITHER_PARALLEL_BAND_WIDTH=9         SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4         SIXEL_THREADS=6         run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" >"${ARTIFACT_LOCAL_DIR}/tall.out" || {
    fail 1 "pipeline run failed (overrides)"
    exit 0
}

pass 1 "pipeline run succeeded (overrides)"

exit 0
