#!/bin/sh
# Verify builtin loader rejects PIC mixed RLE extended count truncation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_pic="${TOP_SRCDIR}/tests/data/corrupted/pic_mixed_rle_ext_truncated_reject.pic"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_pic}" >/dev/null && {
    echo "not ok" 1 - "PIC mixed RLE extended count truncation was unexpectedly accepted"
    exit 0
}

echo "ok" 1 - "PIC mixed RLE extended count truncation is rejected"
exit 0
