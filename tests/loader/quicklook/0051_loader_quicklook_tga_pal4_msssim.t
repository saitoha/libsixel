#!/bin/sh
# TAP test: quicklook decodes paletted TGA(type1-pal4) with stable quality.

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
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type1-pal4.tga" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_tga_pal4.six" || {
    echo "not ok" 1 - "quicklook TGA pal4 decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb-from-pal4.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_tga_pal4.six" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "quicklook TGA pal4 decode preserves quality"
exit 0
