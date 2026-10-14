#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -v60px "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" \
    >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - scielab invalid ppd"
    exit 0
}

echo "ok 1 - scielab ppd rejects trailing garbage"
exit 0
