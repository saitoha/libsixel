#!/bin/sh
# TAP test: static frame with Atkinson dithering from animated GIF (builtin loader).

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -Lbuiltin! -S -datkinson "${image_gif}" >/dev/null || {
    fail 1 "sequence splitting with Atkinson fails"
    exit 0
}

pass 1 "sequence splitting with Atkinson works"
exit 0
