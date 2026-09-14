#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify png override pending error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -T display-p3 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" >/dev/null 2>&1 || status=$?
test "$status" -eq 3 || {
    echo "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
echo "ok 1 - png override pending error"
exit 0
