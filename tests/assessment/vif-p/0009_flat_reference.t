#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P zero reference information is unavailable.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    echo "not ok 1 - VIF-P zero reference information is unavailable"
    exit 0
}
test "${value}" = nan || {
    echo "not ok 1 - VIF-P zero reference information is unavailable"
    exit 0
}
echo "ok 1 - VIF-P zero reference information is unavailable"
exit 0
