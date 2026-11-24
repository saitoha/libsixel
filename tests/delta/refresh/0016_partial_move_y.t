#!/bin/sh
# Verify partial move y.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0016_partial_move_y || {
    printf '\n%s\n' "not ok 1 - partial move y"
    exit 0
}
printf '\n%s\n' "ok 1 - partial move y"
exit 0
