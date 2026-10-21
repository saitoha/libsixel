#!/bin/sh
# Policy: docs/quality/metrics.md
# Multiple metrics and gates share one stdin input
set -eux
echo "1..1"
set -v

status=0
# Both operands are read-only inputs.
# shellcheck disable=SC2094
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m GMSD,PSNR_Y -b GMSD:max=0 -b PSNR_Y:min=99 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" - < "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = '{
  "GMSD": 0.000000,
  "PSNR_Y": 99.000000
}' || {
    echo "not ok 1 - Multiple metrics and gates share one stdin input"
    exit 0
}
echo "ok 1 - Multiple metrics and gates share one stdin input"
exit 0
