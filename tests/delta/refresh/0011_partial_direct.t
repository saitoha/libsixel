#!/bin/sh
# Verify partial direct.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0011_partial_direct || {
    echo "not ok 1 - partial direct"
    exit 0
}
echo "ok 1 - partial direct"
exit 0
