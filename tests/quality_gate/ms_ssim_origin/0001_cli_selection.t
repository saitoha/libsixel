#!/bin/sh
# Policy: docs/quality/metrics.md
# The reference's five 11-pixel windows require a larger fixture than 64x64.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -m ms-ssim-origin -b MS-SSIM-origin:0.98 \
    "${TOP_SRCDIR}/images/snake.png" "${TOP_SRCDIR}/images/snake.png") || {
    echo "not ok 1 - origin metric selection and baseline"
    exit 0
}
test "${value}" = '1.000000' || {
    echo "not ok 1 - origin identity score"
    exit 0
}
echo "ok 1 - origin metric selection and baseline"
exit 0
