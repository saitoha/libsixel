#!/bin/sh
# Verify progressive conflict.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0017_progressive_conflict || {
    printf '\n%s\n' "not ok 1 - progressive conflict"
    exit 0
}
printf '\n%s\n' "ok 1 - progressive conflict"
exit 0
