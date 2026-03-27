#!/bin/sh
# TAP test: gd loader rejects non-integer start-frame environment values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc" \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    echo "not ok" 1 - "gd accepted a non-integer start-frame environment value"
    exit 0
}

echo "ok" 1 - "gd rejects non-integer start-frame environment values"
exit 0
