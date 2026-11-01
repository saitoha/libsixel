#!/bin/sh
# Policy: docs/quality/metrics.md
# Legacy float overflow is rejected
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:1e40 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1 || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - Legacy float overflow is rejected"
    exit 0
}
echo "ok 1 - Legacy float overflow is rejected"
exit 0
