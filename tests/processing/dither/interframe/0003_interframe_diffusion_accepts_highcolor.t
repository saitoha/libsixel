#!/bin/sh
# TAP test ensuring interframe works in high-color mode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -I -d interframe "${input_image}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "high-color mode unexpectedly rejected interframe"
    exit 0
}

echo "ok" 1 - "high-color mode accepts interframe"
exit 0
