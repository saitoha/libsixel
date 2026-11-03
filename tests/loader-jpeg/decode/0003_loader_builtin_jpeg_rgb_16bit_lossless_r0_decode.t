#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin loader can decode RGB 16-bit lossless JPEG (restart=0).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-16bit-lossless.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${input_jpeg}" -o/dev/null || {
    echo "not ok" 1 - "builtin 16-bit lossless JPEG decode failed"
    exit 0
}

echo "ok" 1 - "builtin 16-bit lossless JPEG decode succeeds"
exit 0
