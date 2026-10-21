#!/bin/sh
# Policy: docs/quality/metrics.md
# Unavailable required metric fails a combined gate
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:max=0,MS-SSIM:min=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || status=$?

test "${status}" -eq 5 || {
    echo "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"MS-SSIM is NaN or unavailable"*) true ;; *) false ;; esac || {
    echo "not ok 1 - Unavailable required metric fails a combined gate"
    exit 0
}
echo "ok 1 - Unavailable required metric fails a combined gate"
exit 0
