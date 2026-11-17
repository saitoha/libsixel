#!/bin/sh
# Verify horizontal noop age.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0117_horizontal_noop_age || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal noop age"
    exit 0
}
echo "ok 1 - horizontal noop age"
exit 0
