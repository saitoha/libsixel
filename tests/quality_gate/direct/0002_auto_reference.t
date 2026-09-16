#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct auto reference retains paint-time colors.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 --direct "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six" "${TOP_SRCDIR}/tests/data/color-numeric/stable.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - paint result differs: $value"
    exit 0
}
echo "ok 1 - auto reference"
exit 0
