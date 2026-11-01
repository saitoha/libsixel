#!/bin/sh
# Verify img2sixel accepts a G1 PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite grayscale (color type 0, 1-bit) image for -m.
# - Convert a standard input image with the palette mapfile.
# - Expect img2sixel to exit successfully.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

palette_png="${TOP_SRCDIR}/images/pngsuite/basic/basn0g01.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-g1.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${palette_png}" "${input_png}" \
        >"${target_sixel}" || {
    echo "not ok" 1 - "G1 PNG palette mapfile rejected"
    exit 0
}

echo "ok" 1 - "G1 PNG palette mapfile accepted"

exit 0
