#!/bin/sh
# Scale with Lanczos4 filter and emit palette dump.
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
target_sixel="${output_dir}/lanczos4-palette-dump.sixel"

require_file "${snake_jpg}"

if run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "Lanczos4 scaling emits palette dump"
else
    fail 1 "Lanczos4 scaling palette dump fails"
fi

exit "${status}"
