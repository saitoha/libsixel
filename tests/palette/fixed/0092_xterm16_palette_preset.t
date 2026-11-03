#!/bin/sh
# Re-encode Sixel using xterm16 palette preset.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bxterm16 "${snake_six}" -o/dev/null || {
    echo "not ok" 1 - "xterm16 preset failed"
    exit 0
}

echo "ok" 1 - "xterm16 preset re-encodes Sixel"

exit 0
