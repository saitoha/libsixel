#!/bin/sh
# Verify partial ahead.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0012_partial_ahead || {
    printf '\n%s\n' "not ok 1 - partial ahead"
    exit 0
}
printf '\n%s\n' "ok 1 - partial ahead"
exit 0
