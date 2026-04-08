#!/bin/sh
# TAP test: gdk-pixbuf2 rejects negative out-of-range start-frame env.

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
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=-2147483649" \
    -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}" 2>&1 >/dev/null)" && {
    echo "not ok 1 - gdk-pixbuf2 accepted negative out-of-range env"
    exit 0
}

test "${error_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.}" != \
    "${error_text}" || {
    echo "not ok 1 - gdk-pixbuf2 negative out-of-range message mismatch"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 rejects negative out-of-range env"
exit 0
