#!/bin/sh
# Verify builtin loader can decode RGB 16-bit lossless JPEG (restart=0).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-16bit-lossless.jpg"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_jpeg_16bit_lossless.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin 16-bit lossless JPEG decode failed"
    exit 0
}

echo "ok" 1 - "builtin 16-bit lossless JPEG decode succeeds"
exit 0
