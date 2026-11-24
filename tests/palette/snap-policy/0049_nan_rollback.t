#!/bin/sh
# Verify native snap: nan rollback.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 0 0 1 4 || {
    printf '\n%s\n' "not ok 1 - nan rollback"
    exit 0
}

printf '\n%s\n' "ok 1 - nan rollback"
exit 0
