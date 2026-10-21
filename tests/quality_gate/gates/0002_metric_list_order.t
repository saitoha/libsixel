#!/bin/sh
# Policy: docs/quality/metrics.md
# Comma lists emit metrics in registry order
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m " PSNR_Y, GMSD " "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = '{
  "GMSD": 0.000000,
  "PSNR_Y": 99.000000
}' || {
    echo "not ok 1 - Comma lists emit metrics in registry order"
    exit 0
}
echo "ok 1 - Comma lists emit metrics in registry order"
exit 0
