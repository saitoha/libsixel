#!/bin/sh
# Policy: docs/loader/README.md
# Verify explicit loaders lead a deduplicated open plan before defaults.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0124_loader_manager_open_plan" || {
    echo "not ok 1 - loader manager open plan"
    exit 0
}

echo "ok 1 - loader manager open plan is ordered and deduplicated"
exit 0
