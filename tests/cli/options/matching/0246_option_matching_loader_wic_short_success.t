#!/bin/sh
# TAP test verifying the WIC loader suboption short form is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lwic:I40! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico" \
    -o/dev/null || {
    echo "not ok" 1 - "WIC loader short form was rejected"
    exit 0
}

echo "ok" 1 - "WIC loader short form is accepted"
exit 0
