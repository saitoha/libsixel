#!/bin/sh
# TAP test verifying every k-center suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:Aauto:Plegacy:S1:Qlegacy:F256:Elegacy:Zlegacy:X1:N1:I1:H3:B64:R0:U1.0:Dlegacy:Y1.0:K1:Mfull:T0:J0.0:Gauto:O1:L0:Cauto:Voff:Wsoft \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "k-center suboption short forms were rejected"
    exit 0
}

echo "ok" 1 - "all k-center suboption short forms are accepted"
exit 0
