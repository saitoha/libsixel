#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none --ppd=60 -m s-ciede2000 -m S-CIEDE2000 -b S-CIEDE2000:0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    printf '\n%s\n' "not ok 1 - spatial DE00 mean identity"
    exit 0
}
test "${value}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - spatial DE00 mean identity value"
    exit 0
}

printf '\n%s\n' "ok 1 - spatial DE00 mean identity and duplicate selection"
exit 0
