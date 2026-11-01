#!/bin/sh
# TAP test ensuring img2sixel rejects missing complexion-score argument.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -C </dev/null >/dev/null 2>&1 && {
    echo "not ok" 1 - "unexpected success: missing complexion score argument"
    exit 0
}

echo "ok" 1 - "missing complexion score argument rejected"
exit 0
