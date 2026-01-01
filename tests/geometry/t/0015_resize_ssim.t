#!/bin/sh
# Validate that resized output remains visually close to a fixed baseline
# using SSIM/PSNR metrics produced by assessment/lsqa.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")

artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/resize.log"
sixel_out="${artifact_dir}/resize.six"
png_out="${artifact_dir}/resize.png"
baseline_png="${artifact_dir}/baseline.png"

mkdir -p "${artifact_dir}"

export SIXEL_THREADS=1

script_dir=${test_dir}
. "${script_dir}/../../common/t/0001_converters_common.t"
quality_helper="${top_srcdir}/tests/common/quality.sh"
. "${quality_helper}"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

quality_require_lsqa

echo "1..1"

input_png="${top_srcdir}/tests/data/inputs/formats/resize_source.png"
require_file "${input_png}"
cp "${top_srcdir}/tests/data/baseline/resize_source_50.png" "${baseline_png}"

if run_img2sixel -w 50% -h 50% -o "${sixel_out}" "${input_png}" \
        2>>"${log_file}" \
        && run_sixel2png -o "${png_out}" "${sixel_out}" \
        && quality_compare_images "${baseline_png}" "${png_out}" 0.95 28 \
            "${log_file}"; then
    echo "ok 1 - resized output matches baseline (SSIM/PSNR)"
else
    echo "not ok 1 - resized output diverged from baseline"
    exit 1
fi
