#!/bin/sh
# Verify controller cancellation boundary: end-success.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job end-success || {
    printf '\n%s\n' "not ok 1 - color controller end-success"
    exit 0
}
printf '\n%s\n' "ok 1 - color controller end-success"
exit 0
