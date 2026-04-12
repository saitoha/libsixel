#!/bin/sh
# TAP test: explicit_first policy should let -B override embedded PNG bKGD.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
expected_sixel="${ARTIFACT_LOCAL_DIR}/libpng_bgan6a08_white.six"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_bgan6a08_white.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -B#fff -Llibpng:cms_engine=none! "${input_png}" >"${expected_sixel}" || {
    echo "not ok" 1 - "libpng baseline conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_BACKGROUND_POLICY=explicit_first \
              -B#fff -Lbuiltin:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin bKGD cli override conversion failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${expected_sixel}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "builtin explicit_first policy overrides embedded bKGD (matches libpng)"
exit 0
