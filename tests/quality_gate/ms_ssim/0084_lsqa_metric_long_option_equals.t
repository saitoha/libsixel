#!/bin/sh
# Verify --metrics=NAME works and matches -m NAME output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value_short=$(run_lsqa -m MS-SSIM "${image_ref}" "${image_out}") || {
    fail 1 "failed to calculate metric with -m"
    exit 0
}
value_long=$(run_lsqa --metrics=MS-SSIM "${image_ref}" "${image_out}") || {
    fail 1 "failed to calculate metric with --metrics="
    exit 0
}

test "${value_short}" = "${value_long}" || {
    fail 1 "--metrics= output does not match -m"
    exit 0
}

pass 1 "--metrics= returns same value as -m"
exit 0
