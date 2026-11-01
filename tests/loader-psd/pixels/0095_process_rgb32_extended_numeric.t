#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process rgb32 extended numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0094_process_rgb32_extended_numeric 1>&2 || {
    echo "not ok 1 - process rgb32 extended numeric"
    exit 0
}
echo "ok 1 - process rgb32 extended numeric"
exit 0
