#!/bin/sh
# Policy: docs/quality/metrics.md
# Legacy color bounds retain minimum semantics
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b DELTA_E00:1 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || status=$?

test "${status}" -eq 5 || {
    echo "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *below*) true ;; *) false ;; esac || {
    echo "not ok 1 - Legacy color bounds retain minimum semantics"
    exit 0
}
echo "ok 1 - Legacy color bounds retain minimum semantics"
exit 0
