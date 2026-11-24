#!/bin/sh
# Verify same rgb different definition.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0003_same_rgb_different_definition" || {
    printf '\n%s\n' "not ok 1 - same rgb different definition"
    exit 0
}
printf '\n%s\n' "ok 1 - same rgb different definition"
exit 0
