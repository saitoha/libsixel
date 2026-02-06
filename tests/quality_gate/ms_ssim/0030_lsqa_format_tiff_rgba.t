#!/bin/sh
# Confirm RGBA TIFF quality meets the MS-SSIM baseline.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

feature_defined_in_config "HAVE_LIBTIFF" || skip_all "libtiff support is disabled in this build"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgba.tiff"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba.six"
if run_img2sixel -Llibtiff! "${image_path}" >"${output_sixel}"; then
    :
else
    fail 1 "tiff rgba conversion failed"
    exit 0
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

exit 0
