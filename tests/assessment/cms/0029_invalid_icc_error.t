#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify invalid icc error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -T "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" >/dev/null 2>&1 || status=$?
test "$status" -eq 3 || {
    printf '\n%s\n' "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
printf '\n%s\n' "ok 1 - invalid icc error"
exit 0
