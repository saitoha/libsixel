#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier_loss_exp17 allocation failure cleanup.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0143_jp_hier_loss_exp17_allocation" 1>&2 || {
    echo "not ok 1 - hier_loss_exp17 allocation cleanup"
    exit 0
}
echo "ok 1 - hier_loss_exp17 allocation cleanup"
exit 0
