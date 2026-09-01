#!/bin/sh
# TAP test verifying every Sierra suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d sierra:V3:Nserpentine \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "Sierra suboption short forms were rejected"
    exit 0
}

echo "ok" 1 - "all Sierra suboption short forms are accepted"
exit 0
