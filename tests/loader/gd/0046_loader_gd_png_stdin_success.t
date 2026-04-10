#!/bin/sh
# TAP test: forced gd loader decodes PNG input from stdin.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    <"${TOP_SRCDIR}/tests/data/inputs/formats/rgb.png" >/dev/null || {
    echo "not ok" 1 - "forced gd loader failed to decode PNG from stdin"
    exit 0
}

echo "ok" 1 - "forced gd loader decodes PNG from stdin"
exit 0
