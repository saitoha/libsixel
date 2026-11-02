#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

LSQA_METRICS=S-CIEDE2000_max
LSQA_BASELINE=S-CIEDE2000_max:max=0
export LSQA_METRICS LSQA_BASELINE
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    echo "not ok 1 - spatial DE00 environment"
    exit 0
}
test "${value}" = 0.000000 || {
    echo "not ok 1 - spatial DE00 environment value"
    exit 0
}

echo "ok 1 - spatial DE00 environment selection and gate"
exit 0
