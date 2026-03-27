#!/bin/sh
# TAP test: builtin PNG path should honor embedded bKGD when compositing alpha.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng loader is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
expected_sixel="${ARTIFACT_LOCAL_DIR}/libpng_bgan6a08_default.six"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_bgan6a08_default.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng:cms_engine=none! "${input_png}" >"${expected_sixel}" || {
    echo "not ok" 1 - "libpng baseline conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin bKGD default composite conversion failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${expected_sixel}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "builtin bKGD default compositing matches libpng baseline"
exit 0
