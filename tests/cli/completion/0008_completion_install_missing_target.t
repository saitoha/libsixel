#!/bin/sh
# TAP test verifying install-completion rejects missing targets.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo '1..1'
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -2 >/dev/null && {
    echo "not ok" 1 - "missing install target unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "missing install target is rejected"
exit 0
