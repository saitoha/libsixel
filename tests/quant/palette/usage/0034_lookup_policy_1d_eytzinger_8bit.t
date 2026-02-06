#!/bin/sh
# Exercise the 8-bit Eytzinger lookup policy through img2sixel options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-8bit.six"

if run_img2sixel --lookup-policy=eytzinger -p 16 -d none \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "8-bit Eytzinger lookup policy completes"
else
    fail 1 "8-bit Eytzinger lookup policy failed"
fi

exit "${status}"
