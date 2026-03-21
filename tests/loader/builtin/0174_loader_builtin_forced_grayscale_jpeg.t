#!/bin/sh
# TAP test confirming --loaders builtin! decodes grayscale JPEG input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/grayscale.jpg"

run_img2sixel -L builtin! "${input_jpeg}" >/dev/null || {
    echo "not ok" 1 - "builtin forced grayscale JPEG decoding failed"
    exit 0
}

echo "ok" 1 - "builtin forced grayscale JPEG decoding succeeds"
exit 0
