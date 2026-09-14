#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct selective_blur retains unpainted background.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -B red -dselective_blur:threshold=24 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/hole.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - paint result differs: $value"
    exit 0
}
echo "ok 1 - background selective blur"
exit 0
