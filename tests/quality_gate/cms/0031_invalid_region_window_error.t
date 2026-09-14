#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify invalid region window error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -w 7 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" >/dev/null 2>&1 || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
echo "ok 1 - invalid region window error"
exit 0
