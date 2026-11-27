#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P is included in the verbose metric report.
set -eux
echo "1..1"
set -v

export LSQA_VERBOSE=1
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W srgb -P float32 -m VIF-P \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" 2>&1) || {
    printf '\n%s\n' "not ok 1 - VIF-P verbose report"
    exit 0
}
case "${value}" in
    *'VIF-P: 1.000000'*) ;;
    *) printf '\n%s\n' "not ok 1 - VIF-P verbose score"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - VIF-P is included in the verbose metric report"
exit 0
