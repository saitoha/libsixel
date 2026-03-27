#!/bin/sh
# TAP test: gd loader decodes interlaced GIF input successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMGIFPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" \
    >/dev/null || {
    echo "not ok" 1 - "gd failed to decode interlaced GIF input"
    exit 0
}

echo "ok" 1 - "gd decodes interlaced GIF input"
exit 0
