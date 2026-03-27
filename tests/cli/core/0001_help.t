#!/bin/sh
# TAP test ensuring img2sixel help command executes successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -H >/dev/null || {
    echo "not ok" 1 - "help output failed"
    exit 0
}

echo "ok" 1 - "help output available"
exit 0
