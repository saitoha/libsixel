#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify libtiff builtin CMS output for RGB parametric ICC fixture (types 3/4).

# Assess the explicitly sized encoder output, after source decoding and
# nearest resampling but before any post-encoding enlargement. The independent
# reference uses the same integer replication; native sample tests stay small.
# Regenerate: python3 tests/data/loader/quality/generate_references.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

# Both build systems publish this loader capability as HAVE_LIBTIFF.
test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_34.tiff"
reference_png="${TOP_SRCDIR}/tests/data/loader/quality/ab2e92a3-tiff-para34-expected.ppm"
output_six="${TMPDIR:-/tmp}/libsixel-rgb-parametric-34-$$.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: rgb_parametric_34.tiff"
    exit 0
}

test -f "${reference_png}" || {
    echo "not ok" 1 - "missing independent reference: tiff-para34-expected.png"
    exit 0
}

trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -~ certlut -r nearest -w 192 -h 192 -Llibtiff:cms_engine=builtin! "${input_tiff}" \
    2>&1 >"${output_six}") || {
    echo "not ok" 1 - "libtiff builtin cms decode failed: rgb_parametric_34"
    exit 0
}

case "${trace}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok" 1 - "libtiff did not deliver the quality input"
    exit 0
}

case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok" 1 - "builtin CMS did not apply to the quality input"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_png}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "rgb_parametric_34 builtin cms fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff builtin cms rgb_parametric_34 keeps MS-SSIM ${lsqa_floor}"
exit 0
