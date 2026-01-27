#!/bin/sh
# Verify TGA type 9 (RLE color-mapped) with 16-color palette.

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

lsqa_common_path="${test_dir}/../../lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.9990}

if ! lsqa_sixel_init "$0"; then
    printf '1..1\n'
    fail 1 "lsqa or img2sixel missing"
    exit "${status}"
fi

artifact_root=${LSQA_ARTIFACT_ROOT}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'

image_path="${LSQA_INPUT_ROOT}/inputs/formats/snake-tga-type9-pal4.tga"
if lsqa_sixel_assert_quality "${image_path}" "snake-tga-type9-pal4.tga" \
    "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "type 9 PAL4 TGA meets lsqa floor"
else
    fail 1 "type 9 PAL4 TGA quality below floor"
fi

exit "${status}"
