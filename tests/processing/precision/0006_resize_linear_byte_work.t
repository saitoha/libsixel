#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify resize linear byte work on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0006_resize_linear_byte_work 1>&2 || {
    echo "not ok 1 - resize linear byte work"
    exit 0
}
echo "ok 1 - resize linear byte work"
exit 0
