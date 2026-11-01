#!/bin/sh
# TAP test ensuring 8-bit output conflicts with pipe mode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -8 -P </dev/null >/dev/null  && {
    echo "not ok" 1 - "unexpected success: 8-bit output conflicts with pipe mode"
    exit 0
}

echo "ok" 1 - "invalid option rejected"
exit 0
