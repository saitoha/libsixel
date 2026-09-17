#!/bin/sh
# Verify terminal color parser s3 only p3.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    s3-only || {
    echo "not ok 1 - terminal color parser s3 only p3"
    exit 0
}

echo "ok 1 - terminal color parser s3 only p3"
exit 0
