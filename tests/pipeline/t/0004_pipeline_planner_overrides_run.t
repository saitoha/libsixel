#!/bin/sh
# TAP test: pipeline planner runs with override environment variables.

set -eux

export SIXEL_THREADS=6

artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/pipeline.log"
ppm_tall="${artifact_dir}/grid_tall.ppm"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"
. "${script_dir}/../../lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_tall_ppm "${ppm_tall}"

if SIXEL_DITHER_PARALLEL_THREADS_MAX=1 \
        SIXEL_DITHER_PARALLEL_BAND_WIDTH=9 \
        SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4 \
        run_img2sixel -v -o "${artifact_dir}/tall.six" "${ppm_tall}" \
        >"${artifact_dir}/tall.out" 2>"${log_file}"; then
    printf 'ok 1 - pipeline run succeeded (overrides)\n'
else
    printf 'not ok 1 - pipeline run failed (overrides)\n'
    exit 1
fi
