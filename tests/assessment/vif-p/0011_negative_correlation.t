#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P negative correlation transmits zero information.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W srgb -P float32 -m VIF-P \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/assessment/vif-p-inverse.ppm") || {
    printf '\n%s\n' "not ok 1 - VIF-P negative correlation transmits zero information"
    exit 0
}
test "${value}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - VIF-P negative correlation transmits zero information"
    exit 0
}
printf '\n%s\n' "ok 1 - VIF-P negative correlation transmits zero information"
exit 0
