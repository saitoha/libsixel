#!/bin/sh
# Confirm PGM encode flag cooperates with palette auto-selection.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_pgm="${TOP_SRCDIR}/tests/data/inputs/snake_64.pgm"

run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null || {
    echo "not ok" 1 "PGM encode flag failed"
    exit 0
}

echo "ok" 1 "PGM encode flag cooperates with palette auto-selection"

exit 0
