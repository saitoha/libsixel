#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier loss exp0 samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0086_jp_hier_loss_exp0" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier loss exp0 samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier loss exp0 samples"
exit 0
