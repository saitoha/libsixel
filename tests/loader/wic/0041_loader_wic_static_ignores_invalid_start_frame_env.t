#!/bin/sh
# TAP test: wic static decode ignores invalid start-frame env values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8.png"
error_text=''

error_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc" \
    -L wic! -ldisable \
    "${input_png}" 2>&1 >/dev/null)" || {
    echo "not ok 1 - wic static decode failed with invalid env"
    exit 0
}

test "${error_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO*}" = \
    "${error_text}" || {
    echo "not ok 1 - wic static decode emitted start-frame env error text"
    exit 0
}

echo "ok 1 - wic static decode ignores invalid start-frame env values"
exit 0
