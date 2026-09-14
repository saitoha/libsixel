#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct reference definition remains independent.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -Rdisplay-p3 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%.*}" -ge 6 || {
    echo "not ok 1 - input definition was ignored: $value"
    exit 0
}
echo "ok 1 - reference definition"
exit 0
