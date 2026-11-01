#!/bin/sh
# Policy: docs/quality/metrics.md
# Metric aliases deduplicate to scalar output
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m GMSD,gmsd -m gmsd "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = 0.000000 || {
    echo "not ok 1 - Metric aliases deduplicate to scalar output"
    exit 0
}
echo "ok 1 - Metric aliases deduplicate to scalar output"
exit 0
