#!/bin/sh
# Confirm PGM encode flag cooperates with palette auto-selection.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_pgm="${images_dir}/snake.pgm"

if run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null; then
    pass 1 "PGM encode flag cooperates with palette auto-selection"
else
    fail 1 "PGM encode flag failed"
fi

exit "${status}"
