#!/bin/sh
# Verify ladder rebind reset.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0137_ladder_rebind_reset" || {
    printf '\n%s\n' "not ok 1 - ladder rebind reset"
    exit 0
}
printf '\n%s\n' "ok 1 - ladder rebind reset"
exit 0
