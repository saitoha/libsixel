#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Dequantize none does not disable direct drawing.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -dnone "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six") || {
    printf '\n%s\n' "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - paint result differs: $value"
    exit 0
}
printf '\n%s\n' "ok 1 - dequantize none"
exit 0
