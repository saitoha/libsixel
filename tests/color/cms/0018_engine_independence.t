#!/bin/sh
# Verify engine independence.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0012_engine_independence" || {
    printf '\n%s\n' "not ok 1 - engine independence"
    exit 0
}

printf '\n%s\n' "ok 1 - engine independence"
exit 0
