#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P accepts the complete 41x41 pyramid.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-minimum.ppm" "${TOP_SRCDIR}/tests/data/assessment/vif-p-minimum.ppm") || {
    echo "not ok 1 - VIF-P minimum size"
    exit 0
}
test "${value}" = 1.000000 || {
    echo "not ok 1 - VIF-P minimum identity"
    exit 0
}
echo "ok 1 - VIF-P accepts the complete 41x41 pyramid"
exit 0
