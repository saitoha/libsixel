#!/bin/sh
# TAP test: gdk-pixbuf2 positive start-frame env matches CLI selection.

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
default_text=''
env_text=''
cli_text=''

default_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 default decode failed"
    exit 0
}

env_text="$(SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 env start-frame decode failed"
    exit 0
}

cli_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}")" || {
    echo "not ok 1 - gdk-pixbuf2 --start-frame=1 decode failed"
    exit 0
}

test "${default_text}" != "${env_text}" || {
    echo "not ok 1 - gdk-pixbuf2 env start-frame did not change output"
    exit 0
}

test "${env_text}" = "${cli_text}" || {
    echo "not ok 1 - gdk-pixbuf2 env start-frame mismatched CLI"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 positive start-frame env matches CLI"
exit 0
