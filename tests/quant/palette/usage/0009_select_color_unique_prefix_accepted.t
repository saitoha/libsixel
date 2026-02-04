#!/bin/sh
# Confirm unique select-color prefix is accepted.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"
require_file "${snake_jpg}"

if run_img2sixel -shist "${snake_jpg}" >/dev/null 2>>"${log_file}"; then
    pass 1 "unique select-color prefix accepted"
else
    fail 1 "unique select-color prefix failed"
fi

exit "${status}"
