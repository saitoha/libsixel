#!/bin/sh
# Verify options after positional arguments are reordered and parsed.

# The five-scale metric requires at least 176 pixels in both dimensions.
set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/images/snake.png"
image_out="${TOP_SRCDIR}/images/snake.six"
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
