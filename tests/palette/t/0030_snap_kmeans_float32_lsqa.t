#!/bin/sh
# Run lsqa quality checks for k-means palette snapping with float32 palettes.
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

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${artifact_dir}/snap-kmeans-float32.six"
output_png="${output_dir}/snap-kmeans-float32.png"

require_file "${input_image}"


if SIXEL_PALETTE_SNAP_TARGET_POLICY=nearest \
        SIXEL_PALETTE_SNAP_TIMING_POLICY=all \
        SIXEL_PALETTE_SNAP_APPROACH_RATE=0.7 \
        SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=0.7 \
        run_img2sixel -Q kmeans -6 -W oklab \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel snap kmeans float32 failed"
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
            "snap-kmeans-float32" "${lsqa_run_status}"
        if [ -s "${lsqa_err_file}" ]; then
            printf '# lsqa stderr follows\n'
            sed 's/^/# /' "${lsqa_err_file}"
        else
            printf '# %s: lsqa produced no diagnostics\n' \
                "snap-kmeans-float32"
        fi
    fi
    rm -f "${lsqa_err_file}"
    [ ${lsqa_run_status} -eq 0 ]; }; then
    pass 1 "snap kmeans float32 lsqa passed"
else
    fail 1 "snap kmeans float32 lsqa failed"
fi

exit "${status}"
