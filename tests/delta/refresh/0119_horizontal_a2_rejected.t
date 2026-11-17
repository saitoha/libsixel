#!/bin/sh
# Verify horizontal a2 rejected.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0119_horizontal_a2_rejected || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal a2 rejected"
    exit 0
}
echo "ok 1 - horizontal a2 rejected"
exit 0
