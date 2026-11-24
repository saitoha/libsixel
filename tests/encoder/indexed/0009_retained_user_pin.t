#!/bin/sh
# An active body consumer blocks commit.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0009_retained_user_pin" || {
    printf '\n%s\n' 'not ok 1 - retained_user_pin'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_user_pin'
exit 0
