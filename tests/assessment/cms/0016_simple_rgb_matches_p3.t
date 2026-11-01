#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify simple rgb matches p3.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -R display-p3 -T "rgb:.3127,.329,.68,.32,.265,.69,.15,.06:srgb" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - unexpected numeric color difference"
    exit 0
}
echo "ok 1 - simple rgb matches p3"
exit 0
