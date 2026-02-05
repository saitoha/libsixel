#!/bin/sh
# TAP test: pipeline planner runs with override environment variables.

set -eux

export SIXEL_THREADS=6

ppm_tall="${ARTIFACT_LOCAL_DIR}/grid_tall.ppm"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_tall_ppm "${ppm_tall}"

if SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
        SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
        SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
        run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" \
        >"${ARTIFACT_LOCAL_DIR}/tall.out"; then
    printf 'ok 1 - pipeline run succeeded (overrides)\n'
else
    printf 'not ok 1 - pipeline run failed (overrides)\n'
    exit 1
fi
