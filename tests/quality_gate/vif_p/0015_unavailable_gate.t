#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P unavailable result fails even a zero bound.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P -b VIF-P:min=0 \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || status=$?
test "${status}" -eq 5 && test "${value}" = nan || {
    echo "not ok 1 - VIF-P unavailable gate"
    exit 0
}
echo "ok 1 - VIF-P unavailable result fails even a zero bound"
exit 0
