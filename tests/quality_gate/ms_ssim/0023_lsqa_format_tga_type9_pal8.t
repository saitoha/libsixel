#!/bin/sh
# Verify TGA type 9 (RLE color-mapped) with 256-color palette.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.9990}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/snake-tga-type9-pal8.tga"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
if run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}"; then
    :
else
    fail 1 "type 9 PAL8 TGA quality below floor"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "type 9 PAL8 TGA meets lsqa floor"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "type 9 PAL8 TGA quality below floor"
fi

exit "${status}"
