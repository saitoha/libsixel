#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m S-CIELAB -b S-CIELAB:0 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.jpg" \
    >/dev/null || status=$?
test "${status}" -eq 5 || {
    echo "not ok 1 - scielab upper baseline"
    exit 0
}

echo "ok 1 - scielab error above ceiling fails"
exit 0
