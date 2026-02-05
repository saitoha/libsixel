#!/bin/sh
# Check explicit dimensions and palette options work together.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"
snake_dims="${tmp_dir}/snake-dims.sixel"



if run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" \
    <"${snake_jpg}"; then
    pass 1 "explicit dimensions and palette options succeeded"
else
    fail 1 "explicit dimensions and palette options failed"
fi

exit "${status}"
