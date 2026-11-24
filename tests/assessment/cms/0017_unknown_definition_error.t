#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify unknown definition error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -T unknown "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" >/dev/null 2>&1 || status=$?
test "$status" -eq 2 || {
    printf '\n%s\n' "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
printf '\n%s\n' "ok 1 - unknown definition error"
exit 0
