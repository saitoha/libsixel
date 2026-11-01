#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Sparse P3 paint keeps earlier colors and unpainted background.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -B blue -Rdisplay-p3 -Tdisplay-p3 "${TOP_SRCDIR}/tests/data/color-numeric/sparse.six" "${TOP_SRCDIR}/tests/data/color-numeric/sparse-redefined.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - paint result differs: $value"
    exit 0
}
echo "ok 1 - sparse p3"
exit 0
