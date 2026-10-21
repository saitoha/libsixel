#!/bin/sh
# Policy: docs/quality/metrics.md
# Environment lists select and gate multiple metrics
set -eux
echo "1..1"
set -v

status=0
value=$(LSQA_METRICS='PSNR_Y,GMSD' LSQA_BASELINE='GMSD:max=0,PSNR_Y:min=99' ${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none  "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = '{
  "GMSD": 0.000000,
  "PSNR_Y": 99.000000
}' || {
    echo "not ok 1 - Environment lists select and gate multiple metrics"
    exit 0
}
echo "ok 1 - Environment lists select and gate multiple metrics"
exit 0
