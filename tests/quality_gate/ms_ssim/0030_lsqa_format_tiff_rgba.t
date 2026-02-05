#!/bin/sh
# Confirm RGBA TIFF quality meets the MS-SSIM baseline.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_LIBTIFF"; then
    skip_all "libtiff support is disabled in this build"
fi

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgba.tiff"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba.six"
if run_img2sixel -Llibtiff! "${image_path}" >"${output_sixel}"; then
    :
else
    fail 1 "tiff rgba conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "tiff rgba quality meets baseline"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "tiff rgba quality regressed"
fi

exit "${status}"
