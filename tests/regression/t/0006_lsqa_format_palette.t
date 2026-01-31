#!/bin/sh
# Verify palette PNG quality respects the lsqa baseline and relaxed floor.

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

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.0}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/palette.png"
output_sixel="${artifact_dir}/palette.six"
if run_img2sixel -Lbuiltin "${image_path}" >"${output_sixel}"; then
    :
else
    fail 1 "palette quality regressed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "palette quality meets baseline"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "palette quality regressed"
fi

exit "${status}"
