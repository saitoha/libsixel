#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

LSQA_VERBOSE=1
export LSQA_VERBOSE
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m S-CIEDE2000,S-CIEDE2000_max "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" 2>&1) || {
    echo "not ok 1 - spatial DE00 verbose report"
    exit 0
}
case "${value}" in
    *'S-CIEDE2000: 0.000000'*'S-CIEDE2000_max: 0.000000'*) ;;
    *) echo "not ok 1 - spatial DE00 verbose values"; exit 0 ;;
esac

echo "ok 1 - spatial DE00 verbose values"
exit 0
