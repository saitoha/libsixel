#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify linear byte boundary on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0002_linear_byte_boundary 1>&2 || {
    echo "not ok 1 - linear byte boundary"
    exit 0
}
echo "ok 1 - linear byte boundary"
exit 0
