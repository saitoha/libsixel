#!/bin/sh
# TAP test: static frame with Atkinson dithering from animated GIF (builtin loader).

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -Lbuiltin! -S -datkinson "${image_gif}" >/dev/null || {
    fail 1 "sequence splitting with Atkinson fails"
    exit 0
}

pass 1 "sequence splitting with Atkinson works"
exit 0
