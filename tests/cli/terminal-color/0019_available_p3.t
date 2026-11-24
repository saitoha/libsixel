#!/bin/sh
# Verify terminal color parser available p3.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    p3 || {
    printf '\n%s\n' "not ok 1 - terminal color parser available p3"
    exit 0
}

printf '\n%s\n' "ok 1 - terminal color parser available p3"
exit 0
