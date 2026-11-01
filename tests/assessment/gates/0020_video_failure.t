#!/bin/sh
# Policy: docs/quality/metrics.md
# Video diagnoses all failed mean bounds
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -Ma -b PSNR_Y:min=100,GMSD:max=-1 "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" 2>&1) || status=$?

test "${status}" -eq 5 || {
    echo "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"PSNR_Y "*below*"GMSD "*above*) true ;; *) false ;; esac || {
    echo "not ok 1 - Video diagnoses all failed mean bounds"
    exit 0
}
echo "ok 1 - Video diagnoses all failed mean bounds"
exit 0
