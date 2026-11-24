#!/bin/sh
# Verify partial return.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0020_partial_return || {
    printf '\n%s\n' "not ok 1 - partial return"
    exit 0
}
printf '\n%s\n' "ok 1 - partial return"
exit 0
