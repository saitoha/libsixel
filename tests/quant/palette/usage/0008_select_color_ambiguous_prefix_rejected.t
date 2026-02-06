#!/bin/sh
# Ensure ambiguous select-color prefix is rejected.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"

if run_img2sixel -sa "${snake_jpg}" >/dev/null; then
    fail 1 "ambiguous select-color prefix accepted"
else
    pass 1 "ambiguous select-color prefix rejected"
fi

exit "${status}"
