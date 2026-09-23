#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier loss exp17 samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0089_jp_hier_loss_exp17" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier loss exp17 samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier loss exp17 samples"
exit 0
