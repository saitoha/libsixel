#!/bin/sh
# Policy: docs/quality/metrics.md
# Nonfinite bound is rejected
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:max=nan "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1 || status=$?

test "${status}" -eq 2 || {
    printf '\n%s\n' "not ok 1 - Nonfinite bound is rejected"
    exit 0
}
printf '\n%s\n' "ok 1 - Nonfinite bound is rejected"
exit 0
