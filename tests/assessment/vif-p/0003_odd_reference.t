#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P odd reference.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P -b VIF-P:min=0.495972151663,VIF-P:max=0.496412151663 \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-odd-reference.ppm" "${TOP_SRCDIR}/tests/data/assessment/vif-p-odd-distorted.ppm" >/dev/null || {
    echo "not ok 1 - VIF-P odd reference"
    exit 0
}
echo "ok 1 - VIF-P odd reference"
exit 0
