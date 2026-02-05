#!/bin/sh
# Exercise the float32 VPTE lookup policy through img2sixel options.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/vpte-float32.six"

if run_img2sixel --lookup-policy=vpte --precision=float32 -p 16 -d none \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "float32 VPTE lookup policy completes"
else
    fail 1 "float32 VPTE lookup policy failed"
fi

exit "${status}"
