#!/bin/sh
# Policy: docs/quality/metrics.md
# Explicit lists replace malformed environment lists
set -eux
echo "1..1"
set -v

status=0
value=$(LSQA_METRICS='invalid,,' LSQA_BASELINE='invalid,,' ${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m GMSD -b GMSD:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

{ test "${status}" -eq 0 && test "${value}" = 0.000000; } || {
    printf '\n%s\n' "not ok 1 - Explicit lists replace malformed environment lists"
    exit 0
}
printf '\n%s\n' "ok 1 - Explicit lists replace malformed environment lists"
exit 0
