#!/bin/sh
# TAP test verifying every k-medoids suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:Aauto:S1:I1:M0:T1:K0:J1:N0:D1:E8:X8:H3:B64:R0:U1.0:Q0:Y2:Gauto:O1:L0:Cauto:Voff:Wsoft \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "k-medoids suboption short forms were rejected"
    exit 0
}

echo "ok" 1 - "all k-medoids suboption short forms are accepted"
exit 0
