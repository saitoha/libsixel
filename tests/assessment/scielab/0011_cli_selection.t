#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m s-cielab -b S-CIELAB:0.01 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    echo "not ok 1 - scielab selection"
    exit 0
}
test "${value}" = 0.000000 || {
    echo "not ok 1 - scielab identity"
    exit 0
}

echo "ok 1 - scielab selection and upper baseline"
exit 0
