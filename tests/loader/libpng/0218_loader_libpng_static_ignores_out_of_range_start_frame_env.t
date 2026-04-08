#!/bin/sh
# TAP test: libpng static decode ignores out-of-range env values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8.png"
error_text=''

error_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=2147483648" \
    -L libpng! -ldisable \
    "${input_png}" 2>&1 >/dev/null)" || {
    echo "not ok 1 - libpng static decode failed with out-of-range env"
    exit 0
}

test "${error_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO*}" = \
    "${error_text}" || {
    echo "not ok 1 - libpng static decode emitted out-of-range env error text"
    exit 0
}

echo "ok 1 - libpng static decode ignores out-of-range env values"
exit 0
