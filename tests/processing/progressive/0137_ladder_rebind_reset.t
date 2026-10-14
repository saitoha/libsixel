#!/bin/sh
# Verify ladder rebind reset.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0137_ladder_rebind_reset" || {
    echo "not ok 1 - ladder rebind reset"
    exit 0
}
echo "ok 1 - ladder rebind reset"
exit 0
