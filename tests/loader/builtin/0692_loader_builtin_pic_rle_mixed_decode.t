#!/bin/sh
# Verify builtin loader decodes PIC mixed RLE packets.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_pic="${TOP_SRCDIR}/tests/data/inputs/formats/pic_valid_rle_mixed_rgb_5x2.pic"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_pic}" >/dev/null || {
    echo "not ok" 1 - "builtin PIC mixed RLE decode failed"
    exit 0
}

echo "ok" 1 - "builtin PIC mixed RLE decode succeeded"
exit 0
