#!/bin/sh
# Check palette PNG stability across repeated lsqa runs.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

printf '1..1\n'
set -v

image1="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image2="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"

value0=$(run_lsqa -m ms-ssim "${image1}" "${image2}")
value1=$(run_lsqa -m ms-ssim "${image1}" "${image2}")
value2=$(run_lsqa -m ms-ssim "${image1}" "${image2}")

if "${value0}" != "${value1}" || test "${value1}" != "${value2}"; then
    fail 1 "palette repeat variance exceeded tolerance"
    exit 0
fi

pass 1 "palette repeat variance within tolerance"
exit 0
