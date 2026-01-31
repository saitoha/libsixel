#!/bin/sh
# TAP test covering the -R/--gri-limit option with deterministic encoding.
#
# Flow:
# - Encode a reference input with a single encoder thread (-=1).
# - Encode the same input with -R/--gri-limit enabled and -=1.
# - Compare outputs with lsqa MS-SSIM and require a perfect match.

set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
lsqa_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_common.sh
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
. "${conversion_common_path}"
. "${lsqa_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

lsqa_floor=1.0

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
case_id=${test_name%.t}
output_plain="${output_dir}/${case_id}-plain.six"
output_limited="${output_dir}/${case_id}-limited.six"

require_file "${input_image}"

if run_img2sixel -=1 -o "${output_plain}" "${input_image}" 2>>"${log_file}" && \
        run_img2sixel -=1 --gri-limit -o "${output_limited}" \
        "${input_image}" 2>>"${log_file}" && \
        {
            lsqa_err_file=$(mktemp)
            lsqa_run_status=0
            if ! run_lsqa -b "MS-SSIM:${lsqa_floor}" \
                "${input_image}" "${output_sixel}" > /dev/null \
                2>"${lsqa_err_file}"; then
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
    pass 1 "gri-limit deterministic output matches"
else
    fail 1 "gri-limit deterministic output mismatch"
fi

exit "${status}"
