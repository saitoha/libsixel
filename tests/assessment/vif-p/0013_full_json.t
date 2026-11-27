#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P appears in the full JSON report.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W srgb -P float32  \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    printf '\n%s\n' "not ok 1 - VIF-P full JSON"
    exit 0
}
case "${value}" in
    *'"VIF-P": 1.000000'*) ;;
    *) printf '\n%s\n' "not ok 1 - VIF-P full JSON field"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - VIF-P appears in the full JSON report"
exit 0
