#!/bin/sh
# Guard quarter-resolution square PNG quality against lsqa baselines.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")


lsqa_common_path="${test_dir}/../../lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"


status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

if ! lsqa_sixel_init "$0"; then
    printf '1..1\n'
    fail 1 "lsqa or img2sixel missing"
    exit "${status}"
fi

artifact_root=${LSQA_ARTIFACT_ROOT}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'
set -v

image_path="${LSQA_INPUT_ROOT}/resolutions/quarter_square.png"
output_sixel="${artifact_dir}/quarter_square.six"
if run_img2sixel -Lbuiltin "${image_path}" >"${output_sixel}" && \
    lsqa_run_benchmark "${image_path}" "${output_sixel}" \
        "quarter_square.png" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "quarter_square quality meets baseline"
else
    fail 1 "quarter_square quality regressed"
fi

exit "${status}"
