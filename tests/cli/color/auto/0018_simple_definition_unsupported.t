#!/bin/sh
# Verify terminal color parser simple definition unsupported.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    simple-definition || {
    echo "not ok 1 - terminal color parser simple definition unsupported"
    exit 0
}

echo "ok 1 - terminal color parser simple definition unsupported"
exit 0
