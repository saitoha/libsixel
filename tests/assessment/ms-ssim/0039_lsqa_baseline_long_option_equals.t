#!/bin/sh
# Verify --baseline=METRIC:VALUE accepts the long option with '='.

# The five-scale metric requires at least 176 pixels in both dimensions.
# The small non-identity PNG/SIXEL pair retains parser and exit-status coverage.
set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_192.png"
image_out="${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192-baseline.six"
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --metrics=MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate baseline source metric"
    exit 0
}

cr=$(printf '\r')
value=${value%"$cr"}

${SIXEL_RUNTIME-} "${LSQA_PATH}" --metrics=MS-SSIM --baseline="MS-SSIM:${value}" "${image_ref}" "${image_out}" >/dev/null || {
    echo "not ok" 1 - "--baseline= should accept METRIC:VALUE"
    exit 0
}

echo "ok" 1 - "--baseline= accepted METRIC:VALUE"
exit 0
