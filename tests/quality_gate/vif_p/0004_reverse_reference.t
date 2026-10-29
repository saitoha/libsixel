#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P reference order.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P -b VIF-P:min=0.512168752434,VIF-P:max=0.512608752434 \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-distorted.ppm" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" >/dev/null || {
    echo "not ok 1 - VIF-P reference order"
    exit 0
}
echo "ok 1 - VIF-P reference order"
exit 0
