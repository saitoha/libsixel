#!/bin/sh
# Policy: docs/quality/metrics.md
# Aliases cannot duplicate a bound
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b MS_SSIM:0 -b SSIM:min=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || status=$?

test "${status}" -eq 2 || {
    printf '\n%s\n' "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"baseline already specified"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - Aliases cannot duplicate a bound"
    exit 0
}
printf '\n%s\n' "ok 1 - Aliases cannot duplicate a bound"
exit 0
