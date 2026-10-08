#!/bin/sh
# Policy: docs/quality/metrics.md
# An undersized comparison must not satisfy even a zero quality baseline.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -m ms-ssim-origin -b ms-ssim-origin:0 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || status=$?
test "${status}" -eq 5 && test "${value}" = nan || {
    echo "not ok 1 - unavailable origin cannot pass baseline"
    exit 0
}
echo "ok 1 - unavailable origin cannot pass baseline"
exit 0
