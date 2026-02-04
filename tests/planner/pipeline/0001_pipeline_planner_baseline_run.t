#!/bin/sh
# TAP test: pipeline planner runs baseline case with verbose dump.

set -eux

export SIXEL_THREADS=4

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/pipeline.log"
ppm_small="${artifact_dir}/grid_small.ppm"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_small_ppm "${ppm_small}"

if run_img2sixel -v -o "${artifact_dir}/small.six" "${ppm_small}" \
        >"${artifact_dir}/small.out" 2>"${log_file}"; then
    printf 'ok 1 - pipeline run succeeded (baseline)\n'
else
    printf 'not ok 1 - pipeline run failed (baseline)\n'
    exit 1
fi
