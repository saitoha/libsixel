#!/bin/sh
# Verify terminal color parser payload counter saturation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    saturation || {
    echo "not ok 1 - terminal color parser payload counter saturation"
    exit 0
}

echo "ok 1 - terminal color parser payload counter saturation"
exit 0
