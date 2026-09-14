#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify unreadable icc error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -T "icc:${TOP_SRCDIR}/tests/data/color-numeric/no-such.icc" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" >/dev/null 2>&1 || status=$?
test "$status" -eq 3 || {
    echo "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
echo "ok 1 - unreadable icc error"
exit 0
