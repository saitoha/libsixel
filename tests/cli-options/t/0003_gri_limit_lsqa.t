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

if ! lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

echo "1..1"
set -v

input_image="${LSQA_INPUT_ROOT}/inputs/snake_64.png"
case_id=${test_name%.t}
output_plain="${output_dir}/${case_id}-plain.six"
output_limited="${output_dir}/${case_id}-limited.six"

require_file "${input_image}"

if run_img2sixel -=1 -o "${output_plain}" "${input_image}" 2>>"${log_file}" && \
        run_img2sixel -=1 --gri-limit -o "${output_limited}" \
        "${input_image}" 2>>"${log_file}" && \
        lsqa_run_benchmark "${output_plain}" "${output_limited}" \
        "${case_id}" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "gri-limit deterministic output matches"
else
    fail 1 "gri-limit deterministic output mismatch"
fi

exit "${status}"
