#!/bin/sh
# Policy: docs/quality/metrics.md
# Structural and color gates pass quietly
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b MS-SSIM:min=0.98 -b DE00_region_max:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" "${TOP_SRCDIR}/tests/data/inputs/snake_192.png") || status=$?

test "${status}" -eq 0 && test -z "${value}" || {
    echo "not ok 1 - Structural and color gates pass quietly"
    exit 0
}
echo "ok 1 - Structural and color gates pass quietly"
exit 0
