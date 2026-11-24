#!/bin/sh
# Verify partial latest.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0013_partial_latest || {
    printf '\n%s\n' "not ok 1 - partial latest"
    exit 0
}
printf '\n%s\n' "ok 1 - partial latest"
exit 0
