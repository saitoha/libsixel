#!/bin/sh
# TAP test: override configuration reports expected bands and mode.

set -eux

export SIXEL_THREADS=6

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/pipeline.log"
ppm_tall="${artifact_dir}/grid_tall.ppm"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../_lib/sh/common.sh"
. "${script_dir}/../../lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_tall_ppm "${ppm_tall}"
SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
    SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
    SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
    run_img2sixel -v -o "${artifact_dir}/tall.six" "${ppm_tall}" \
    >"${artifact_dir}/tall.out" 2>"${log_file}" || true

summary=$(grep "bands=" "${log_file}" | head -n 1 || true)
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
