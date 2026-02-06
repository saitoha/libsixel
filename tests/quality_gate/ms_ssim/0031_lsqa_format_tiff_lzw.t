#!/bin/sh
# Confirm LZW-compressed TIFF quality meets the MS-SSIM baseline.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

feature_defined_in_config "HAVE_LIBTIFF" || skip_all "libtiff support is disabled in this build"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgb-lzw.tiff"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgb_lzw.six"
run_img2sixel -Llibtiff! "${image_path}" >"${output_sixel}" || {
    fail 1 "tiff lzw conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "tiff lzw quality meets baseline"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "tiff lzw quality regressed"
fi

exit 0
