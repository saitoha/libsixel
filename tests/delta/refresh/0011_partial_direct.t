#!/bin/sh
# Verify partial direct.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0011_partial_direct || {
    printf '\n%s\n' "not ok 1 - partial direct"
    exit 0
}
printf '\n%s\n' "ok 1 - partial direct"
exit 0
