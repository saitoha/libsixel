#!/bin/sh
# Exercise the 8-bit RBC lookup policy through img2sixel options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rbc-8bit.six"

run_img2sixel --lookup-policy=rbc -p 16 -d none \
        -o "${output_sixel}" "${snake_png}" || {
    fail 1 "8-bit RBC lookup policy failed"
    exit 0
}

pass 1 "8-bit RBC lookup policy completes"

exit 0
