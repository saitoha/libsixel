#!/bin/sh
# Verify terminal color parser capability consistency.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    consistency || {
    echo "not ok 1 - terminal color parser capability consistency"
    exit 0
}

echo "ok 1 - terminal color parser capability consistency"
exit 0
