#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify explicit dequantize error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -T display-p3 -d k_undither "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" >/dev/null 2>&1 || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
echo "ok 1 - explicit dequantize error"
exit 0
