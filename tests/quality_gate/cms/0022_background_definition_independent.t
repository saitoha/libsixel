#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify background definition independent.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -R srgb -T display-p3 -B red "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/hole.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - unexpected numeric color difference"
    exit 0
}
echo "ok 1 - background definition independent"
exit 0
