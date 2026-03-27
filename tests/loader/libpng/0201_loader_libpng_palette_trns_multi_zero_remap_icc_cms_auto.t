#!/bin/sh
# Verify libpng multi-zero palette+tRNS matches single-zero under cms=auto.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_multi="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
input_single="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png"
out_multi="${ARTIFACT_LOCAL_DIR}/libpng-multi0-white-cms1.six"
out_single="${ARTIFACT_LOCAL_DIR}/libpng-single0-white-cms1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng:cms_engine=auto! \
              -B#ffffff \
              -d none -p256 \
              "${input_multi}" >"${out_multi}" || {
    echo "not ok 1 - libpng multi-zero cms=auto render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng:cms_engine=auto! \
              -B#ffffff \
              -d none -p256 \
              "${input_single}" >"${out_single}" || {
    echo "not ok 1 - libpng single-zero cms=auto render failed"
    exit 0
}

lsqa_msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:1.0" "${out_single}" "${out_multi}" 2>&1
) || {
    echo "not ok 1 - ${lsqa_msg}"
    exit 0
}

echo "ok 1 - libpng multi-zero remap matches single-zero under cms=auto"
exit 0
