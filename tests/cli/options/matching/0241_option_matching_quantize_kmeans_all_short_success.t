#!/bin/sh
# TAP test verifying every k-means suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:Iauto:T0.12:Bnone:N6:Muniform:Dtrilinear:R32:Foff:Pauto:S1:E1:A1:X20:U0:H0:K1:J1:Gauto:O1:L0:Cauto:Voff:Wsoft \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "k-means suboption short forms were rejected"
    exit 0
}

echo "ok" 1 - "all k-means suboption short forms are accepted"
exit 0
