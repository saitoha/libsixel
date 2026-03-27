#!/bin/sh
# TAP test verifying sixel2png prints version information.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -V >/dev/null || {
    echo "not ok" 1 - "version option failed"
}

echo "ok" 1 - "prints version"
exit 0
