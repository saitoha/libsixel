#!/bin/sh
# Verify controller cancellation boundary: control-error.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job control-error || {
    echo "not ok 1 - color controller control-error"
    exit 0
}
echo "ok 1 - color controller control-error"
exit 0
