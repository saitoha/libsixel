#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P contrast gain above one.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P -b VIF-P:min=1.313661244854,VIF-P:max=1.318861244854 \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-dim.ppm" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" >/dev/null || {
    echo "not ok 1 - VIF-P contrast gain above one"
    exit 0
}
echo "ok 1 - VIF-P contrast gain above one"
exit 0
