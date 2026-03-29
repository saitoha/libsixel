#!/bin/sh
# TAP test: coregraphics decodes HEIC alpha content with stable quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heic-alpha-lossless-64.heic" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_heic_alpha_lossless.six" || {
    echo "not ok" 1 - "coregraphics HEIC alpha decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgba.ppm" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_heic_alpha_lossless.six" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "coregraphics HEIC alpha decode preserves quality"
exit 0
