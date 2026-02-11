#!/bin/sh
# Ensure ambiguous select-color prefix is rejected.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"

run_img2sixel -sa "${snake_jpg}" >/dev/null && {
    fail 1 "ambiguous select-color prefix accepted"
    exit 0
}

pass 1 "ambiguous select-color prefix rejected"

exit 0
