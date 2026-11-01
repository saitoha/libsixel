#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

# Both inputs are read-only; one uses stdin and one opens the same file.
# shellcheck disable=SC2094
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m S-CIELAB "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" - <"${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    echo "not ok 1 - scielab standard input"
    exit 0
}
test "${value}" = 0.000000 || {
    echo "not ok 1 - scielab stdin identity"
    exit 0
}

echo "ok 1 - scielab standard input target"
exit 0
