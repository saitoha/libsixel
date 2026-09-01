#!/bin/sh
# TAP test verifying every interframe suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d interframe:Datkinson:Nserpentine \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null || {
    echo "not ok" 1 - "interframe suboption short forms were rejected"
    exit 0
}

echo "ok" 1 - "all interframe suboption short forms are accepted"
exit 0
