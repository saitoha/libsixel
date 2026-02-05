#!/bin/sh
# Ensure ambiguous select-color prefix is rejected.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"


if run_img2sixel -sa "${snake_jpg}" >/dev/null; then
    fail 1 "ambiguous select-color prefix accepted"
else
    pass 1 "ambiguous select-color prefix rejected"
fi

exit "${status}"
