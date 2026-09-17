#!/bin/sh
# Verify controller cancellation boundary: end-error.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job end-error || {
    echo "not ok 1 - color controller end-error"
    exit 0
}
echo "ok 1 - color controller end-error"
exit 0
