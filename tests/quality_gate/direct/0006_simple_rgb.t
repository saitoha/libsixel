#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct custom RGB matches named P3.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -Rdisplay-p3 -T "rgb:.3127,.329,.68,.32,.265,.69,.15,.06:srgb" "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - paint result differs: $value"
    exit 0
}
echo "ok 1 - simple rgb"
exit 0
