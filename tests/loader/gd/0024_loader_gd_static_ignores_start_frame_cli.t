#!/bin/sh
# TAP test: gd static decode ignores --start-frame overrides.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8.png"
error_text=''

error_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=999 \
    -L gd! -ldisable \
    "${input_png}" 2>&1 >/dev/null)" || {
    echo "not ok 1 - gd static decode failed with --start-frame=999"
    exit 0
}

test "${error_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO*}" = \
    "${error_text}" || {
    echo "not ok 1 - gd static decode emitted --start-frame error text"
    exit 0
}

echo "ok 1 - gd static decode ignores --start-frame overrides"
exit 0
