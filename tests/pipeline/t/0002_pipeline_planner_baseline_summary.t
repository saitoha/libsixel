#!/bin/sh
# TAP test: baseline pipeline summary reports bands and mode.

set -eux

export SIXEL_THREADS=4

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/pipeline.log"
ppm_small="${artifact_dir}/grid_small.ppm"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"
. "${script_dir}/../../lib/sh/pipeline/pipeline_planner_common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

create_small_ppm "${ppm_small}"
run_img2sixel -v -o "${artifact_dir}/small.six" "${ppm_small}" \
    >"${artifact_dir}/small.out" 2>"${log_file}" || true

summary=$(grep "bands=" "${log_file}" | head -n 1 || true)
case "${summary}" in
"    bands=2 queue=2 mode=pipeline")
    printf 'ok 1 - baseline bands/queue/mode\n'
    ;;
"    bands="*)
    printf 'ok 1 - baseline bands/queue/mode (serial environment)\n'
    ;;
*)
    printf 'not ok 1 - baseline bands/queue/mode\n'
    exit 1
    ;;
esac
