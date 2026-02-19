#!/bin/sh
# Verify options after positional arguments are reordered and parsed.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value_prefix=$(run_lsqa -m MS-SSIM "${image_ref}" "${image_out}") || {
    fail 1 "failed to calculate metric with option before args"
    exit 0
}
value_suffix=$(run_lsqa "${image_ref}" "${image_out}" -m MS-SSIM) || {
    fail 1 "failed to calculate metric with option after args"
    exit 0
}

test "${value_prefix}" = "${value_suffix}" || {
    fail 1 "option reordering changed metric value"
    exit 0
}

pass 1 "option reordering supports -m after positional args"
exit 0
