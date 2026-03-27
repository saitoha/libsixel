#!/bin/sh
# Verify builtin loader rejects PSD (invalid RLE stream).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/corrupted/invalid_rle_stream.psd"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null && {
    echo "not ok" 1 - "invalid RLE stream was unexpectedly accepted"
    exit 0
}

echo "ok" 1 - "invalid RLE stream is rejected"
exit 0
