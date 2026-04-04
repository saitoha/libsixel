#!/bin/sh
# TAP test verifying -Q accepts kmedoids base value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmedoids \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null || {
    echo "not ok" 1 - "-Q kmedoids base value was rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts kmedoids base value"
exit 0
