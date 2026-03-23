#!/bin/sh
# Shared single-case TAP logic for libtiff colormgmt fixture checks.

set -eu

: "${CASE_SPACE:?CASE_SPACE is required (rgb or lab)}"
: "${CASE_NAME:?CASE_NAME is required}"
: "${LSQA_FLOOR:?LSQA_FLOOR is required}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/${CASE_SPACE}/${CASE_NAME}.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/${CASE_SPACE}/${CASE_NAME}.six"
output_six="${ARTIFACT_LOCAL_DIR}/${CASE_NAME}_libtiff.six"

if [ ! -f "${input_tiff}" ] || [ ! -f "${reference_six}" ]; then
    echo "not ok" 1 - "missing fixture: ${CASE_SPACE}/${CASE_NAME}"
    exit 0
fi

run_img2sixel -Llibtiff:cms=1! "${input_tiff}" >"${output_six}" || {
    echo "not ok" 1 - "libtiff decode failed: ${CASE_SPACE}/${CASE_NAME}"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${LSQA_FLOOR}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "${CASE_SPACE}/${CASE_NAME}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff colormgmt matches reference: ${CASE_SPACE}/${CASE_NAME}"
exit 0
