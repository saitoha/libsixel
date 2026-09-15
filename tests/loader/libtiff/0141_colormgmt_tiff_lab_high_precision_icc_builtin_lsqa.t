#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify libtiff builtin CMS output for high-precision CIELAB TIFF with Lab ICC.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/lab_high_precision_icc.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/lab_high_precision_icc_tiff_builtin.six"
output_six="${TMPDIR:-/tmp}/libsixel-lab-high-precision-icc-tiff-$$.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: lab_high_precision_icc.tiff"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: lab_high_precision_icc_tiff_builtin.six"
    exit 0
}

trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=builtin! "${input_tiff}" \
    2>&1 >"${output_six}") || {
    echo "not ok" 1 - "libtiff builtin cms decode failed: lab_high_precision_icc.tiff"
    exit 0
}

test "${trace#*"LSXLOAD1|event=ok|loader=libtiff|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok" 1 - "libtiff did not deliver the quality input"
    exit 0
}

test "${trace#*"LSXCMS2|event=apply|engine=builtin|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok" 1 - "builtin CMS did not apply to the quality input"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "lab_high_precision_icc.tiff builtin cms fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff builtin cms lab_high_precision_icc keeps MS-SSIM ${lsqa_floor}"
exit 0
