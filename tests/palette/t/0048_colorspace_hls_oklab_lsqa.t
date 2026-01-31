#!/bin/sh
# Verify HLS input with OKLAB working colorspace meets MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t hls -W oklab.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.99 via lsqa.
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
lsqa_floor=0.98

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${images_dir}/snake.png"
output_sixel="${artifact_dir}/hls-oklab.six"

require_file "${input_image}"


if run_img2sixel -t hls -W oklab -o "${output_sixel}" \
        "${input_image}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel hls+oklab conversion failed"
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
            "hls-oklab" "${lsqa_run_status}"
        if [ -s "${lsqa_err_file}" ]; then
            printf '# lsqa stderr follows\n'
            sed 's/^/# /' "${lsqa_err_file}"
        else
            printf '# %s: lsqa produced no diagnostics\n' \
                "hls-oklab"
        fi
    fi
    rm -f "${lsqa_err_file}"
    [ ${lsqa_run_status} -eq 0 ]; }; then
    pass 1 "hls+oklab lsqa passed"
else
    fail 1 "hls+oklab lsqa failed"
fi

exit "${status}"
