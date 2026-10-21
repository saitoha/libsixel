#!/bin/sh
# Policy: docs/quality/metrics.md
# Color error fails despite passing structure
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b MS-SSIM:min=0,DE00_region_max:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192-baseline.six" 2>&1) || status=$?

test "${status}" -eq 5 || {
    echo "not ok 1 - unexpected exit status"
    exit 0
}
case "${value}" in *"MS-SSIM "*) false ;; *"DE00_region_max "*above*) true ;; *) false ;; esac || {
    echo "not ok 1 - Color error fails despite passing structure"
    exit 0
}
echo "ok 1 - Color error fails despite passing structure"
exit 0
