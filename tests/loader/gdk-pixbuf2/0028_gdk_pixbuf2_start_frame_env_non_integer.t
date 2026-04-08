#!/bin/sh
# TAP test: gdk-pixbuf2 rejects non-integer start-frame env values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
error_text=''

error_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc" \
    -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}" 2>&1 >/dev/null)" && {
    echo "not ok 1 - gdk-pixbuf2 accepted non-integer start-frame env"
    exit 0
}

test "${error_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.}" != \
    "${error_text}" || {
    echo "not ok 1 - gdk-pixbuf2 non-integer env error message mismatch"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 rejects non-integer start-frame env"
exit 0
