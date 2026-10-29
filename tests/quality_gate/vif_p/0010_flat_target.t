#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P flat target transmits zero information.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm") || {
    echo "not ok 1 - VIF-P flat target transmits zero information"
    exit 0
}
test "${value}" = 0.000000 || {
    echo "not ok 1 - VIF-P flat target transmits zero information"
    exit 0
}
echo "ok 1 - VIF-P flat target transmits zero information"
exit 0
