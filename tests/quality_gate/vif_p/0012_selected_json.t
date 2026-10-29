#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P coexists with an unavailable selected metric.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m MS-SSIM,VIF-P \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    echo "not ok 1 - VIF-P selected JSON"
    exit 0
}
test "${value}" = '{
  "MS-SSIM": null,
  "VIF-P": 1.000000
}' || {
    echo "not ok 1 - VIF-P selected JSON"
    exit 0
}
echo "ok 1 - VIF-P coexists with an unavailable selected metric"
exit 0
