#!/bin/sh
# Verify --metrics=NAME works and matches -m NAME output.

# The five-scale metric requires at least 176 pixels in both dimensions.
# The small non-identity PNG/SIXEL pair retains parser and exit-status coverage.
set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_192.png"
image_out="${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192-baseline.six"
value_short=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate metric with -m"
    exit 0
}
value_long=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --metrics=MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate metric with --metrics="
    exit 0
}

test "${value_short}" = "${value_long}" || {
    echo "not ok" 1 - "--metrics= output does not match -m"
    exit 0
}

echo "ok" 1 - "--metrics= returns same value as -m"
exit 0
