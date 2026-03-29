#!/bin/sh
# Verify auto loader path rejects malformed PIC input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_pic="${TOP_SRCDIR}/tests/data/corrupted/pic_packet_size16_reject.pic"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${input_pic}" >/dev/null && {
    echo "not ok" 1 - "auto loader accepted malformed PIC unexpectedly"
    exit 0
}

echo "ok" 1 - "auto loader rejects malformed PIC"
exit 0
