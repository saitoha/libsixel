#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct ICC interpretation changes physical color.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -T "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%.*}" -ge 6 || {
    echo "not ok 1 - input definition was ignored: $value"
    exit 0
}
echo "ok 1 - icc definition"
exit 0
