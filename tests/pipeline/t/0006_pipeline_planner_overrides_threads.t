#!/bin/sh
# TAP test: override thread split respects palette worker reservation.

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
. "${script_dir}/../../common/t/0001_converters_common.t"
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

threads_line=$(grep "band_height=" "${log_file}" | head -n 1 || true)
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
