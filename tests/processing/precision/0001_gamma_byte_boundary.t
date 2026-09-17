#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify gamma byte boundary on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0001_gamma_byte_boundary 1>&2 || {
    echo "not ok 1 - gamma byte boundary"
    exit 0
}
echo "ok 1 - gamma byte boundary"
exit 0
