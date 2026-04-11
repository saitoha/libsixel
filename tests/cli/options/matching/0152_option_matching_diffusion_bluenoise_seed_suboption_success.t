#!/bin/sh
# TAP test verifying -d bluenoise accepts seed suboption.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d bluenoise:seed=42 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null || {
    echo "not ok" 1 - "bluenoise seed suboption was rejected"
    exit 0
}

echo "ok" 1 - "bluenoise seed suboption is accepted"
exit 0
