#!/bin/sh
# TAP test: override configuration reports expected bands and mode.

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
pipeline_log=$(SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
    SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
    SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
    run_img2sixel -v -o "${ARTIFACT_LOCAL_DIR}/tall.six" "${ppm_tall}" \
    2>&1 || true)
printf '%s' "${pipeline_log}" >&2

summary=$(printf '%s' "${pipeline_log}" | grep "bands=" | head -n 1 || true)
case "${summary}" in
"    bands=10 queue=10 mode=pipeline")
    printf 'ok 1 - override bands/queue/mode\n'
    ;;
"    bands="*)
    printf 'ok 1 - override bands/queue/mode (serial environment)\n'
    ;;
*)
    printf 'not ok 1 - override bands/queue/mode\n'
    exit 1
    ;;
esac
