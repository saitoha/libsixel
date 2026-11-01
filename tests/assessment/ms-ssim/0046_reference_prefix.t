#!/bin/sh
# Policy: docs/quality/metrics.md
# The established -mm spelling evaluates the reference product.
set -eux
echo "1..1"
set -v

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -m MS-SSIM "${TOP_SRCDIR}/images/snake.png" \
    "${TOP_SRCDIR}/images/snake.jpg") || {
    echo "not ok 1 - reference metric"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -mm "${TOP_SRCDIR}/images/snake.png" \
    "${TOP_SRCDIR}/images/snake.jpg") || {
    echo "not ok 1 - reference metric prefix"
    exit 0
}
test "${expected}" != nan && test "${expected}" != 1.000000 &&
    test "${expected}" = "${actual}" || {
    echo "not ok 1 - reference prefix changed metric"
    exit 0
}
echo "ok 1 - reference metric prefix"
exit 0
