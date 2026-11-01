#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify single-metric CLI selection without requiring other color metrics.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max -R srgb -T srgb "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - pixel maximum evaluation failed"
    exit 0
}
test "${value}" = 0.000000 || {
    echo "not ok 1 - identical colors must have zero maximum difference"
    exit 0
}
echo "ok 1 - pixel_delta_max"
exit 0
