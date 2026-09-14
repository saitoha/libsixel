#!/bin/sh
# Verify p3 clipping negative control.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0004_p3_clipping_negative_control" || {
    echo "not ok 1 - p3 clipping negative control"
    exit 0
}
echo "ok 1 - p3 clipping negative control"
exit 0
