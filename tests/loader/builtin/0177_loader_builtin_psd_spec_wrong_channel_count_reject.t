#!/bin/sh
# Verify builtin loader rejects PSD (channel count > 16).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/corrupted/wrong_channel_count.psd"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null && {
    echo "not ok" 1 - "channel count > 16 was unexpectedly accepted"
    exit 0
}

echo "ok" 1 - "channel count > 16 is rejected"
exit 0
