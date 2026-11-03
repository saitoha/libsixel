#!/bin/sh
# Verify partial resize.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0015_partial_resize || {
    echo "not ok 1 - partial resize"
    exit 0
}
echo "ok 1 - partial resize"
exit 0
