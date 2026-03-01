#!/bin/sh
# TAP test confirming coregraphics rejects invalid-signature HEIF input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/corrupted/invalid_signature.heif" >/dev/null && {
    echo "not ok" 1 "coregraphics invalid-signature HEIF should fail"
    exit 0
}

echo "ok" 1 "coregraphics invalid-signature HEIF is rejected"
exit 0
