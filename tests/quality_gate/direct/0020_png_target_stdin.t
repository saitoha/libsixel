#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct selection leaves png target stdin loader results unchanged.
# Keep unequal PNG/BMP dimensions and non-identity pixels for loader parity.
set -eux
echo "1..1"
set -v

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - legacy evaluation failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" - <"${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - direct-option evaluation failed"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - non-SIXEL evaluation changed"
    exit 0
}
echo "ok 1 - png target stdin"
exit 0
