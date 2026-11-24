#!/bin/sh
# Verify controller cancellation boundary: restore-error.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job restore-error || {
    printf '\n%s\n' "not ok 1 - color controller restore-error"
    exit 0
}
printf '\n%s\n' "ok 1 - color controller restore-error"
exit 0
