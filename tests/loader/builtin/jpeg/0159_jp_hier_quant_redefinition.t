#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier quant redefinition with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0159_jp_hier_quant_redefinition || {
    echo "not ok 1 - hier quant redefinition"
    exit 0
}
echo "ok 1 - hier quant redefinition"
exit 0
