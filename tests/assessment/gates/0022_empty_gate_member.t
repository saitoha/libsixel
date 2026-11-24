#!/bin/sh
# Policy: docs/quality/metrics.md
# Empty gate member is rejected
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:max=0, "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1 || status=$?

test "${status}" -eq 2 || {
    printf '\n%s\n' "not ok 1 - Empty gate member is rejected"
    exit 0
}
printf '\n%s\n' "ok 1 - Empty gate member is rejected"
exit 0
