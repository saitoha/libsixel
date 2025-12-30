#!/bin/sh
# TAP test: pipeline planner runs baseline case with verbose dump.

set -euxv

export SIXEL_THREADS=4

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/pipeline.log"
ppm_small="${artifact_dir}/grid_small.ppm"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"
. "${script_dir}/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

create_small_ppm "${ppm_small}"

if run_img2sixel -v -o "${artifact_dir}/small.six" "${ppm_small}" \
        >"${artifact_dir}/small.out" 2>"${log_file}"; then
    printf 'ok 1 - pipeline run succeeded (baseline)\n'
else
    printf 'not ok 1 - pipeline run failed (baseline)\n'
    exit 1
fi
