#!/bin/sh
# Verify same rgb different definition.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0003_same_rgb_different_definition" || {
    echo "not ok 1 - same rgb different definition"
    exit 0
}
echo "ok 1 - same rgb different definition"
exit 0
