#!/bin/sh
# TAP test: builtin loader static GIF frame with Atkinson dithering matches reference.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -S -datkinson \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/small_gif_atkinson_static.six" || {
    echo "not ok" 1 - "sequence splitting with Atkinson fails"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/small_gif_atkinson_static_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/small_gif_atkinson_static.six" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "sequence splitting with Atkinson matches static reference"
exit 0
