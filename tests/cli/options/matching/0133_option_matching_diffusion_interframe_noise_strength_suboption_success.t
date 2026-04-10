#!/bin/sh
# TAP test verifying -d interframe accepts noise_strength suboption.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d interframe:strategy=stbn-mask:noise_strength=0.50 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null || {
    echo "not ok" 1 - "interframe noise_strength suboption was rejected"
    exit 0
}

echo "ok" 1 - "interframe noise_strength suboption is accepted"
exit 0
