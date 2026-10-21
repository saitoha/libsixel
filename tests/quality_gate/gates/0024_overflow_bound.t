#!/bin/sh
# Policy: docs/quality/metrics.md
# Overflow bound is rejected
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:max=1e999 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1 || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - Overflow bound is rejected"
    exit 0
}
echo "ok 1 - Overflow bound is rejected"
exit 0
