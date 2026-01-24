#!/bin/sh
# Verify TGA type 11 (RLE grayscale) quality against lsqa baselines.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

lsqa_sixel_common_path="${test_dir}/../../lib/sh/lsqa/lsqa_sixel_common.sh"
. "${test_dir}/../../lib/sh/lsqa/lsqa_sixel_common.sh"

status=0

if ! lsqa_sixel_init "$0"; then
    printf '1..1\n'
    fail 1 "lsqa or img2sixel missing"
    exit "${status}"
fi

artifact_root=${LSQA_ARTIFACT_ROOT}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
mkdir -p "${artifact_dir}"

MS_SSIM_FLOOR=0.98
PSNR_FLOOR=35.0

printf '1..1\n'

image_path="${LSQA_INPUT_ROOT}/inputs/formats/snake-tga-type11-gray.tga"
if lsqa_sixel_assert_quality "${image_path}" "snake-tga-type11-gray.tga" \
    "${artifact_dir}"; then
    pass 1 "type 11 gray TGA meets lsqa baseline"
else
    fail 1 "type 11 gray TGA quality regressed"
fi

exit "${status}"
