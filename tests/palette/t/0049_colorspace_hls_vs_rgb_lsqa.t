#!/bin/sh
# Compare HLS and RGB conversions with MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t hls.
# - Convert the input image with -t rgb.
# - Compare both outputs and enforce MS-SSIM >= 0.99.
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
lsqa_floor=0.99

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${images_dir}/snake.png"
output_hls="${artifact_dir}/hls.six"
output_rgb="${artifact_dir}/rgb.six"

require_file "${input_image}"


if run_img2sixel -t hls -o "${output_hls}" "${input_image}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel hls conversion failed"
    exit "${status}"
fi

if run_img2sixel -t rgb -o "${output_rgb}" "${input_image}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel rgb conversion failed"
    exit "${status}"
fi

if {
    lsqa_err_file=$(mktemp)
    lsqa_run_status=0
    if ! run_lsqa -b "MS-SSIM:${lsqa_floor}" \
        "${output_rgb}" "${output_hls}" > /dev/null \
        2>"${lsqa_err_file}"; then
        lsqa_run_status=$?
        printf '# %s: assessment/lsqa returned %s\n' \
            "hls-vs-rgb" "${lsqa_run_status}"
        if [ -s "${lsqa_err_file}" ]; then
            printf '# lsqa stderr follows\n'
            sed 's/^/# /' "${lsqa_err_file}"
        else
            printf '# %s: lsqa produced no diagnostics\n' \
                "hls-vs-rgb"
        fi
    fi
    rm -f "${lsqa_err_file}"
    [ ${lsqa_run_status} -eq 0 ]; }; then
    pass 1 "hls vs rgb lsqa passed"
else
    fail 1 "hls vs rgb lsqa failed"
fi

exit "${status}"
