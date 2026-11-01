#!/bin/sh
# Policy: docs/loader/README.md
# Verify a trailing bang closes the loader plan after explicit entries.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0125_loader_manager_closed_plan" || {
    echo "not ok 1 - loader manager closed plan"
    exit 0
}

echo "ok 1 - loader manager trailing bang closes the plan"
exit 0
