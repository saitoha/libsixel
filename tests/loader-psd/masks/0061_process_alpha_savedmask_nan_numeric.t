#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha savedmask nan numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0144_process_alpha_savedmask_nan_numeric 1>&2 || {
    echo "not ok 1 - process alpha savedmask nan numeric"
    exit 0
}
echo "ok 1 - process alpha savedmask nan numeric"
exit 0
