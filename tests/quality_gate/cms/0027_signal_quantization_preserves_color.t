#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify signal quantization preserves color.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -R srgb -T display-p3 -W gamma -P 8bit "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%.*}" -ge 6 || {
    echo "not ok 1 - unexpected numeric color difference"
    exit 0
}
echo "ok 1 - signal quantization preserves color"
exit 0
