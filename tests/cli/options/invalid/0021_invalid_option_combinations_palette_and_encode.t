#!/bin/sh
# TAP test palette size and monochrome option cannot be combined.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p16 -e </dev/null >/dev/null  && {
    echo "not ok" 1 - "unexpected success: palette size and monochrome option conflict"
    exit 0
}

echo "ok" 1 - "invalid option rejected"
exit 0
