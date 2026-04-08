#!/bin/sh
# TAP test: libwebp static decode ignores out-of-range integer env values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

image_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"
error_text=''

error_text="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=2147483648" \
    -L libwebp! -ldisable \
    "${image_webp}" 2>&1 >/dev/null)" || {
    echo "not ok 1 - libwebp static decode failed with out-of-range env"
    exit 0
}

test "${error_text#*SIXEL_LOADER_ANIMATION_START_FRAME_NO*}" = \
    "${error_text}" || {
    echo "not ok 1 - libwebp static decode emitted out-of-range env error text"
    exit 0
}

echo "ok 1 - libwebp static decode ignores out-of-range integer env values"
exit 0
