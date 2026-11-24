#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify forward only icc input.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -R "icc:perceptual:${TOP_SRCDIR}/tests/data/color-numeric/forward-lut.icc" -T "icc:perceptual:${TOP_SRCDIR}/tests/data/color-numeric/forward-lut.icc" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    printf '\n%s\n' "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - unexpected numeric color difference"
    exit 0
}
printf '\n%s\n' "ok 1 - forward only icc input"
exit 0
