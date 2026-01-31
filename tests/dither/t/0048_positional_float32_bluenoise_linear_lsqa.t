#!/bin/sh
# TAP test covering positional float32 bluenoise in linear space.
#
# Flow:
# - Convert the 64x64 snake reference with the target dithering options.
# - Decode the sixel output back to PNG for inspection.
# - Run lsqa to ensure quality stays above the minimum floors.

set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
lsqa_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_common.sh
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
. "${conversion_common_path}"
. "${lsqa_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available


echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
case_id=${test_name%.t}
output_sixel="${artifact_dir}/${case_id}.six"
output_png="${output_dir}/${case_id}.png"

require_file "${input_image}"

if run_img2sixel -d bluenoise -y raster --precision=float32 -W linear \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}" && \
        {
            lsqa_err_file=$(mktemp)
            lsqa_run_status=0
            if ! run_lsqa -b "MS-SSIM:${lsqa_floor}" \
                "${input_image}" "${output_sixel}" > /dev/null \
                2>"${lsqa_err_file}"; then
                lsqa_run_status=$?
                printf '# %s: assessment/lsqa returned %s\n' \
                    "${case_id}" "${lsqa_run_status}"
                if [ -s "${lsqa_err_file}" ]; then
                    printf '# lsqa stderr follows\n'
                    sed 's/^/# /' "${lsqa_err_file}"
                else
                    printf '# %s: lsqa produced no diagnostics\n' \
                        "${case_id}"
                fi
            fi
            rm -f "${lsqa_err_file}"
            [ ${lsqa_run_status} -eq 0 ]; }; then
    pass 1 "positional float32 bluenoise linear lsqa passed"
else
    fail 1 "positional float32 bluenoise linear lsqa failed"
fi

exit "${status}"
