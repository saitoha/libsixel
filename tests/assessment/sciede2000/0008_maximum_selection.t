#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -v28.5 -m s-ciede2000_m -b S-CIEDE2000_max:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    printf '\n%s\n' "not ok 1 - spatial DE00 maximum selection"
    exit 0
}
test "${value}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - spatial DE00 maximum selection value"
    exit 0
}

printf '\n%s\n' "ok 1 - spatial DE00 high-bit maximum and unique prefix"
exit 0
