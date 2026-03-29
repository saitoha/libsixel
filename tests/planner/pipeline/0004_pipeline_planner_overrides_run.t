#!/bin/sh
# TAP test: pipeline planner runs with override environment variables.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

ppm_tall="${TOP_SRCDIR}/tests/data/inputs/tall.ppm"

SIXEL_DITHER_PARALLEL_THREADS_MAX=1         SIXEL_DITHER_PARALLEL_BAND_WIDTH=9         SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4         SIXEL_THREADS=6         ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" >"${ARTIFACT_LOCAL_DIR}/tall.out" || {
    echo "not ok" 1 - "pipeline run failed (overrides)"
    exit 0
}

echo "ok" 1 - "pipeline run succeeded (overrides)"

exit 0
