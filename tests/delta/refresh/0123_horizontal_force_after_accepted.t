#!/bin/sh
# Verify horizontal force after accepted.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0123_horizontal_force_after_accepted || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal force after accepted"
    exit 0
}
echo "ok 1 - horizontal force after accepted"
exit 0
