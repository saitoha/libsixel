#!/bin/sh
# Verify terminal color parser unavailable.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    unavailable || {
    printf '\n%s\n' "not ok 1 - terminal color parser unavailable"
    exit 0
}

printf '\n%s\n' "ok 1 - terminal color parser unavailable"
exit 0
