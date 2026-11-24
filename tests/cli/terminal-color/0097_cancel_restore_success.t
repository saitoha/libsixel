#!/bin/sh
# Verify controller cancellation boundary: restore-success.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job restore-success || {
    printf '\n%s\n' "not ok 1 - color controller restore-success"
    exit 0
}
printf '\n%s\n' "ok 1 - color controller restore-success"
exit 0
