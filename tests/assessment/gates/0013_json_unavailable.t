#!/bin/sh
# Policy: docs/quality/metrics.md
# Selected JSON uses null for unavailable metrics
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m MS-SSIM,GMSD "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = '{
  "MS-SSIM": null,
  "GMSD": 0.000000
}' || {
    echo "not ok 1 - Selected JSON uses null for unavailable metrics"
    exit 0
}
echo "ok 1 - Selected JSON uses null for unavailable metrics"
exit 0
