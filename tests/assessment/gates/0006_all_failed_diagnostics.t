#!/bin/sh
# Policy: docs/quality/metrics.md
# Every failed bound is diagnosed
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b PSNR_Y:min=100 -b GMSD:max=-1 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || status=$?

test "${status}" -eq 5 || {
    printf '\n%s\n' "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"PSNR_Y "*below*"GMSD "*above*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - Every failed bound is diagnosed"
    exit 0
}
printf '\n%s\n' "ok 1 - Every failed bound is diagnosed"
exit 0
