#!/bin/sh
# Verify native snap: cell cielab nearest partial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 1 3 1 1 || {
    printf '\n%s\n' "not ok 1 - cell cielab nearest partial"
    exit 0
}

printf '\n%s\n' "ok 1 - cell cielab nearest partial"
exit 0
