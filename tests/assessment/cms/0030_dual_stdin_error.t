#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify dual stdin error.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max - - >/dev/null 2>&1 || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - invalid interpretation was not rejected"
    exit 0
}
echo "ok 1 - dual stdin error"
exit 0
