#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m S-CIELAB_m -v28.5 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    printf '\n%s\n' "not ok 1 - scielab max prefix"
    exit 0
}
test "${value}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - scielab max value"
    exit 0
}

printf '\n%s\n' "ok 1 - scielab maximum unique prefix"
exit 0
