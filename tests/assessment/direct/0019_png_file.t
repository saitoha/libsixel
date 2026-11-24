#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct selection leaves png file loader results unchanged.
# Keep unequal PNG/BMP dimensions and non-identity pixels for loader parity.
set -eux
echo "1..1"
set -v

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' "not ok 1 - legacy evaluation failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' "not ok 1 - direct-option evaluation failed"
    exit 0
}
test "$actual" = "$expected" || {
    printf '\n%s\n' "not ok 1 - non-SIXEL evaluation changed"
    exit 0
}
printf '\n%s\n' "ok 1 - png file"
exit 0
