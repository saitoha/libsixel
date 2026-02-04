#!/bin/sh
# Check explicit dimensions and palette options work together.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"
snake_dims="${tmp_dir}/snake-dims.sixel"

require_file "${snake_jpg}"

if run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" \
    <"${snake_jpg}" 2>>"${log_file}"; then
    pass 1 "explicit dimensions and palette options succeeded"
else
    fail 1 "explicit dimensions and palette options failed"
fi

exit "${status}"
