#!/bin/sh
# Verify options after positional arguments are reordered and parsed.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value_prefix=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate metric with option before args"
    exit 0
}
value_suffix=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" "${image_ref}" "${image_out}" -m MS-SSIM) || {
    echo "not ok" 1 - "failed to calculate metric with option after args"
    exit 0
}

test "${value_prefix}" = "${value_suffix}" || {
    echo "not ok" 1 - "option reordering changed metric value"
    exit 0
}

echo "ok" 1 - "option reordering supports -m after positional args"
exit 0
