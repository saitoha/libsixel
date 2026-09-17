#!/bin/sh
# Verify controller cancellation boundary: end-success.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job end-success || {
    echo "not ok 1 - color controller end-success"
    exit 0
}
echo "ok 1 - color controller end-success"
exit 0
