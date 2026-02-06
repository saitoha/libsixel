#!/bin/sh
# Exercise the 8-bit VP-tree lookup policy through img2sixel options.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/vptree-8bit.six"

if run_img2sixel --lookup-policy=vptree -p 16 -d none \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "8-bit VP-tree lookup policy completes"
else
    fail 1 "8-bit VP-tree lookup policy failed"
fi

exit "${status}"
