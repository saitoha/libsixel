#!/bin/sh
# TAP test verifying every STBN suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d stbn:Spmj:Dnone:T0.055:Nserpentine:M0:C0:E0:A0:P0:F0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "STBN suboption short forms were rejected"
    exit 0
}

echo "ok" 1 - "all STBN suboption short forms are accepted"
exit 0
