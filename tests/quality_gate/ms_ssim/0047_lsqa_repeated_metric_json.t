#!/bin/sh
# Policy: docs/quality/metrics.md
# Repeated metrics produce named JSON
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m PSNR_Y -m GMSD "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = '{
  "GMSD": 0.000000,
  "PSNR_Y": 99.000000
}' || {
    echo "not ok 1 - Repeated metrics produce named JSON"
    exit 0
}
echo "ok 1 - Repeated metrics produce named JSON"
exit 0
