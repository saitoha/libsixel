#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha zip16 numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0152_process_alpha_zip16_numeric 1>&2 || {
    echo "not ok 1 - process alpha zip16 numeric"
    exit 0
}
echo "ok 1 - process alpha zip16 numeric"
exit 0
