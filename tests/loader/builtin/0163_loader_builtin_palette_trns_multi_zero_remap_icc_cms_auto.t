#!/bin/sh
# Verify builtin multi-zero palette+tRNS matches single-zero under cms=auto.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_multi="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
input_single="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png"
out_multi="${ARTIFACT_LOCAL_DIR}/builtin-multi0-white-cms1.six"
out_single="${ARTIFACT_LOCAL_DIR}/builtin-single0-white-cms1.six"

run_img2sixel -Lbuiltin:cms_engine=auto! \
              -B#ffffff \
              -d none -p256 \
              "${input_multi}" >"${out_multi}" || {
    echo "not ok 1 - builtin multi-zero cms=auto render failed"
    exit 0
}

run_img2sixel -Lbuiltin:cms_engine=auto! \
              -B#ffffff \
              -d none -p256 \
              "${input_single}" >"${out_single}" || {
    echo "not ok 1 - builtin single-zero cms=auto render failed"
    exit 0
}

lsqa_msg=$(
    set +xv
    run_lsqa -m MS-SSIM -b "MS-SSIM:1.0" "${out_single}" "${out_multi}" 2>&1
) || {
    echo "not ok 1 - ${lsqa_msg}"
    exit 0
}

echo "ok 1 - builtin multi-zero remap matches single-zero under cms=auto"
exit 0
