#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P rejects a finite score below its bound.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -b VIF-P:min=0.9 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/assessment/vif-p-distorted.ppm" >/dev/null 2>&1 || status=$?
test "${status}" -eq 5 || {
    echo "not ok 1 - VIF-P finite gate"
    exit 0
}
echo "ok 1 - VIF-P rejects a finite score below its bound"
exit 0
