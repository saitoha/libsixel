#!/bin/sh
# Run lsqa quality checks for k-means final merge with float32 palettes.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

lsqa_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_common.sh
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/resolutions/tiny_square.png"
output_sixel="${artifact_dir}/merge-kmeans-float32.six"
output_png="${output_dir}/merge-kmeans-float32.png"

require_file "${input_image}"


SIXEL_PALETTE_OVERSPLIT_FACTOR=2.2
SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=2
SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5
SIXEL_PALETTE_KMEANS_THRESHOLD=0.1
SIXEL_PALETTE_LUMIN_FACTOR_R=0.3
SIXEL_PALETTE_LUMIN_FACTOR_G=0.4
SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.6
export SIXEL_PALETTE_OVERSPLIT_FACTOR
export SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT
export SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX
export SIXEL_PALETTE_KMEANS_THRESHOLD
export SIXEL_PALETTE_LUMIN_FACTOR_R
export SIXEL_PALETTE_LUMIN_FACTOR_G
export SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L

if run_img2sixel -Q kmeans -F ward -W oklab \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel merge kmeans float32 failed"
    exit "${status}"
fi

if {
    lsqa_err_file=$(mktemp)
    lsqa_run_status=0
    if ! run_lsqa -b "MS-SSIM:${lsqa_floor}" \
        "${input_image}" "${output_sixel}" > /dev/null \
        2>"${lsqa_err_file}"; then
        lsqa_run_status=$?
        printf '# %s: assessment/lsqa returned %s\n' \
            "merge-kmeans-float32" "${lsqa_run_status}"
        if [ -s "${lsqa_err_file}" ]; then
            printf '# lsqa stderr follows\n'
            sed 's/^/# /' "${lsqa_err_file}"
        else
            printf '# %s: lsqa produced no diagnostics\n' \
                "merge-kmeans-float32"
        fi
    fi
    rm -f "${lsqa_err_file}"
    [ ${lsqa_run_status} -eq 0 ]; }; then
    pass 1 "merge kmeans float32 lsqa passed"
else
    fail 1 "merge kmeans float32 lsqa failed"
fi

exit "${status}"
