#!/bin/sh
# Policy: docs/quality/metrics.md
# Structure fails despite passing color
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b MS-SSIM:min=1.01,DE00_region_max:max=1000 "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192-baseline.six" 2>&1) || status=$?

test "${status}" -eq 5 || {
    echo "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"DE00_region_max "*) false ;; *"MS-SSIM "*below*) true ;; *) false ;; esac || {
    echo "not ok 1 - Structure fails despite passing color"
    exit 0
}
echo "ok 1 - Structure fails despite passing color"
exit 0
