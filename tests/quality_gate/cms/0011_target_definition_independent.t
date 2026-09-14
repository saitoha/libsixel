#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify target definition independent.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -T display-p3 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%.*}" -ge 6 || {
    echo "not ok 1 - unexpected numeric color difference"
    exit 0
}
echo "ok 1 - target definition independent"
exit 0
