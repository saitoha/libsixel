#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Target holes use sRGB background independently of P3.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -B red -Rsrgb -Tdisplay-p3 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/hole.six") || {
    printf '\n%s\n' "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - paint result differs: $value"
    exit 0
}
printf '\n%s\n' "ok 1 - target background"
exit 0
