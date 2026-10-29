#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P accepts the existing metric and baseline environment.
set -eux
echo "1..1"
set -v

export LSQA_METRICS=vif-p LSQA_BASELINE=VIF-P:min=1
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32  \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    echo "not ok 1 - VIF-P environment"
    exit 0
}
test "${value}" = 1.000000 || {
    echo "not ok 1 - VIF-P environment score"
    exit 0
}
echo "ok 1 - VIF-P accepts the existing metric and baseline environment"
exit 0
