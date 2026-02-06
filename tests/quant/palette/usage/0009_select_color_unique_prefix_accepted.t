#!/bin/sh
# Confirm unique select-color prefix is accepted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"

if run_img2sixel -shist "${snake_jpg}" >/dev/null; then
    pass 1 "unique select-color prefix accepted"
else
    fail 1 "unique select-color prefix failed"
fi

exit "${status}"
