#!/bin/sh
# TAP test: gdk-pixbuf2 CLI start-frame overrides invalid env values.

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
cli_text=''
cli_bad_env_text=''

cli_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}" 2>&1)" || {
    echo "not ok 1 - gdk-pixbuf2 --start-frame=1 baseline failed"
    exit 0
}

cli_bad_env_text="$(SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}" 2>&1)" || {
    echo "not ok 1 - invalid env unexpectedly overrode --start-frame"
    exit 0
}

test "${cli_bad_env_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.}" = \
    "${cli_bad_env_text}" || {
    echo "not ok 1 - invalid env error leaked despite --start-frame override"
    exit 0
}

test "${cli_text}" = "${cli_bad_env_text}" || {
    echo "not ok 1 - invalid env changed output despite --start-frame"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 --start-frame overrides invalid env"
exit 0
