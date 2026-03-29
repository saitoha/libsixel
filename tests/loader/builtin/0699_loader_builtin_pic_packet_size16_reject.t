#!/bin/sh
# Verify builtin loader rejects PIC packets with size != 8.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_pic="${TOP_SRCDIR}/tests/data/corrupted/pic_packet_size16_reject.pic"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_pic}" >/dev/null && {
    echo "not ok" 1 - "PIC packet size!=8 was unexpectedly accepted"
    exit 0
}

echo "ok" 1 - "PIC packet size!=8 is rejected"
exit 0
