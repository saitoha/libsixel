#!/bin/sh
# TAP test: quicklook decodes grayscale TGA(type3) with stable quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type3-gray.tga" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_tga_type3_gray.six" || {
    echo "not ok" 1 - "quicklook grayscale TGA(type3) decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_tga_type3_gray.six" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "quicklook grayscale TGA(type3) decode preserves quality"
exit 0
