#!/bin/sh
# TAP test: gdk-pixbuf2 CLI start-frame overrides out-of-range env values.

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
cli_oob_env_text=''

cli_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}" 2>&1)" || {
    echo "not ok 1 - gdk-pixbuf2 --start-frame=1 baseline failed"
    exit 0
}

cli_oob_env_text="$(SIXEL_LOADER_ANIMATION_START_FRAME_NO=999999999999999999999999999999 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -L gdk-pixbuf2! -ldisable -S \
    "${input_gif}" 2>&1)" || {
    echo "not ok 1 - out-of-range env unexpectedly overrode --start-frame"
    exit 0
}

test "${cli_oob_env_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.}" = \
    "${cli_oob_env_text}" || {
    echo "not ok 1 - out-of-range env error leaked despite --start-frame"
    exit 0
}

test "${cli_text}" = "${cli_oob_env_text}" || {
    echo "not ok 1 - out-of-range env changed output despite --start-frame"
    exit 0
}

echo "ok 1 - gdk-pixbuf2 --start-frame overrides out-of-range env"
exit 0
