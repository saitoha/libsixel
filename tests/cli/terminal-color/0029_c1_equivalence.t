#!/bin/sh
# Verify terminal color parser c1 equivalence.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    c1 || {
    printf '\n%s\n' "not ok 1 - terminal color parser c1 equivalence"
    exit 0
}

printf '\n%s\n' "ok 1 - terminal color parser c1 equivalence"
exit 0
