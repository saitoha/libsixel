#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P identity selection is case-insensitive.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m vIf-p -b vIf-p:min=1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    echo "not ok 1 - VIF-P identity selection"
    exit 0
}
test "${value}" = 1.000000 || {
    echo "not ok 1 - VIF-P identity score"
    exit 0
}
echo "ok 1 - VIF-P identity selection is case-insensitive"
exit 0
