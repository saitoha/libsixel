#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct selection leaves png reference stdin loader results unchanged.
set -eux
echo "1..1"
set -v

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -L builtin "${TOP_SRCDIR}/images/snake.png" "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp") || {
    echo "not ok 1 - legacy evaluation failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -L builtin - "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" <"${TOP_SRCDIR}/images/snake.png") || {
    echo "not ok 1 - direct-option evaluation failed"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - non-SIXEL evaluation changed"
    exit 0
}
echo "ok 1 - png reference stdin"
exit 0
