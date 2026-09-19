#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify byte request linear source on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0002_byte_request_linear_source 1>&2 || {
    echo "not ok 1 - byte request linear source"
    exit 0
}
echo "ok 1 - byte request linear source"
exit 0
