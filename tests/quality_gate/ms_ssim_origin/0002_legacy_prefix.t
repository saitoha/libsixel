#!/bin/sh
# Policy: docs/quality/metrics.md
# Preserve -mm as legacy MS-SSIM when the new metric shares its prefix.
set -eux
echo "1..1"
set -v

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -m MS-SSIM "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - legacy metric"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -mm "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - legacy metric prefix"
    exit 0
}
test "${expected}" = "${actual}" || {
    echo "not ok 1 - legacy prefix changed metric"
    exit 0
}
echo "ok 1 - legacy metric prefix"
exit 0
