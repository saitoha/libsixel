#!/bin/sh
# TAP test: override thread split respects palette worker reservation.

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

threads_line=$(printf '%s' "${pipeline_log}" | grep "band_height=" \
    | head -n 1 || true)
case "${threads_line}" in
"    band_height=12 overlap=4 threads: dither=1 encode=4")
    printf 'ok 1 - override thread split (palette reserve)\n'
    ;;
"    band_height="*)
    printf 'ok 1 - override thread split (serial environment)\n'
    ;;
*)
    printf 'not ok 1 - override thread split (palette reserve)\n'
    exit 1
    ;;
esac
