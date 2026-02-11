#!/bin/sh
# Confirm PGM encode flag cooperates with palette auto-selection.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_pgm="${top_srcdir}/tests/data/inputs/snake_64.pgm"

run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null || {
    fail 1 "PGM encode flag failed"
    exit 0
}

pass 1 "PGM encode flag cooperates with palette auto-selection"

exit 0
