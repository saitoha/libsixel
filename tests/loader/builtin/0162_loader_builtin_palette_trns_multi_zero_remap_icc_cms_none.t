#!/bin/sh
# Verify builtin multi-zero palette+tRNS matches single-zero under cms=none.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_multi="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
input_single="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png"
out_multi="${ARTIFACT_LOCAL_DIR}/builtin-multi0-white-cms0.six"
out_single="${ARTIFACT_LOCAL_DIR}/builtin-single0-white-cms0.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! \
              -B#ffffff \
              -d none -p256 \
              "${input_multi}" >"${out_multi}" || {
    echo "not ok 1 - builtin multi-zero cms=none render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! \
              -B#ffffff \
              -d none -p256 \
              "${input_single}" >"${out_single}" || {
    echo "not ok 1 - builtin single-zero cms=none render failed"
    exit 0
}

lsqa_msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:1.0" "${out_single}" "${out_multi}" 2>&1
) || {
    echo "not ok 1 - ${lsqa_msg}"
    exit 0
}

echo "ok 1 - builtin multi-zero remap matches single-zero under cms=none"
exit 0
