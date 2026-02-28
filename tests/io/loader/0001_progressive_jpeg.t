#!/bin/sh
# TAP test confirming progressive JPEG decoding works end-to-end.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

progressive_jpeg="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"

run_img2sixel "${progressive_jpeg}" >/dev/null || {
    echo "not ok" 1 "progressive JPEG conversion failed"
    exit 0
}

echo "ok" 1 "progressive JPEG converts"
exit 0
