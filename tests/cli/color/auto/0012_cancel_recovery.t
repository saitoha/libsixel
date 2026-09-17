#!/bin/sh
# Verify terminal color parser cancel recovery.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    cancel || {
    echo "not ok 1 - terminal color parser cancel recovery"
    exit 0
}

echo "ok 1 - terminal color parser cancel recovery"
exit 0
