#!/bin/sh
# Policy: docs/quality/metrics.md
# Both inclusive bounds may gate one metric
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:min=0,GMSD:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

{ test "${status}" -eq 0 && test -z "${value}"; } || {
    printf '\n%s\n' "not ok 1 - Both inclusive bounds may gate one metric"
    exit 0
}
printf '\n%s\n' "ok 1 - Both inclusive bounds may gate one metric"
exit 0
