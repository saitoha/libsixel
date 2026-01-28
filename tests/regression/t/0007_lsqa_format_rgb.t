#!/bin/sh
# Confirm RGB PNG quality meets the lsqa baseline thresholds.

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

if ! lsqa_init "$0"; then
    printf '1..1\n'
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

artifact_root=${LSQA_ARTIFACT_ROOT}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'
set -v

image_path="${LSQA_INPUT_ROOT}/inputs/formats/rgb.png"
if lsqa_assert_quality "${image_path}" "${image_path}" "rgb.png" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "rgb quality meets baseline"
else
    fail 1 "rgb quality regressed"
fi

exit "${status}"
