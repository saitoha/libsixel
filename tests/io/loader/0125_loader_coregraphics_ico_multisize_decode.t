#!/bin/sh
# TAP test: coregraphics loader decodes multi-size ICO input successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico" >/dev/null || {
    fail 1 "coregraphics failed to decode multi-size ICO input"
    exit 0
}

pass 1 "coregraphics decodes multi-size ICO input"
exit 0
