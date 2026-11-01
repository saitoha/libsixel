#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Reference holes use sRGB background independently of P3.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -B red -Rdisplay-p3 -Tsrgb "${TOP_SRCDIR}/tests/data/color-numeric/hole.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - paint result differs: $value"
    exit 0
}
echo "ok 1 - reference background"
exit 0
