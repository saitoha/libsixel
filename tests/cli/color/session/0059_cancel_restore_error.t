#!/bin/sh
# Verify controller cancellation boundary: restore-error.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job restore-error || {
    echo "not ok 1 - color controller restore-error"
    exit 0
}
echo "ok 1 - color controller restore-error"
exit 0
