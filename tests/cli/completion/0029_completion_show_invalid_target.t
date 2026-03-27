#!/bin/sh
# TAP test verifying show-completion rejects unknown targets.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo '1..1'
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -1 fish >/dev/null && {
    echo "not ok" 1 - "invalid show target unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "invalid show target is rejected"
exit 0
