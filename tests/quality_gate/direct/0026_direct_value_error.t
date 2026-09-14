#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Reject direct value.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" --direct=yes "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" >/dev/null 2>&1 || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - invalid combination was not rejected"
    exit 0
}

echo "ok 1 - direct value error"
exit 0
