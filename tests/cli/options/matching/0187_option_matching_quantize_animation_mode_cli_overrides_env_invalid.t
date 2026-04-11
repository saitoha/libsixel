#!/bin/sh
# TAP test verifying CLI animation_mode overrides invalid env values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

SIXEL_PALETTE_ANIMATION_MODE=2 \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:animation_mode=1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli animation_mode did not override invalid env value"
    exit 0
}

echo "ok" 1 - "cli animation_mode overrides invalid env value"
exit 0
