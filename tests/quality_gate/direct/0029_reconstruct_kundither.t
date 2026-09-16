#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct kundither target reconstruction matches the established opaque result through stdin.
set -eux
echo "1..1"
set -v

expected=$(
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_undither -S120 -e25 -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" -o - |
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" -
) || {
    echo "not ok 1 - reconstruction reference failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -L builtin -dk_undither -S120 -e25 "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" - <"${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - direct reconstruction failed"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - reconstruction mismatch: $actual != $expected"
    exit 0
}
echo "ok 1 - reconstruct kundither"
exit 0
