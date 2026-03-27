#!/bin/sh
# Verify --baseline=METRIC:VALUE accepts the long option with '='.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --metrics=MS-SSIM "${image_ref}" "${image_out}" | tr -d \\r) || {
    echo "not ok" 1 - "failed to calculate baseline source metric"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" --baseline="MS-SSIM:${value}" "${image_ref}" "${image_out}" >/dev/null || {
    echo "not ok" 1 - "--baseline= should accept METRIC:VALUE"
    exit 0
}

echo "ok" 1 - "--baseline= accepted METRIC:VALUE"
exit 0
