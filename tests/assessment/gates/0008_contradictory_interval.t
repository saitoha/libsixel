#!/bin/sh
# Policy: docs/quality/metrics.md
# Contradictory bounds are rejected
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:min=1,GMSD:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || status=$?

test "${status}" -eq 2 || {
    printf '\n%s\n' "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"minimum exceeds maximum"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - Contradictory bounds are rejected"
    exit 0
}
printf '\n%s\n' "ok 1 - Contradictory bounds are rejected"
exit 0
