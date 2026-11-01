#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P rejects height below 41.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-short.ppm" "${TOP_SRCDIR}/tests/data/assessment/vif-p-short.ppm") || {
    echo "not ok 1 - VIF-P rejects height below 41"
    exit 0
}
test "${value}" = nan || {
    echo "not ok 1 - VIF-P rejects height below 41"
    exit 0
}
echo "ok 1 - VIF-P rejects height below 41"
exit 0
