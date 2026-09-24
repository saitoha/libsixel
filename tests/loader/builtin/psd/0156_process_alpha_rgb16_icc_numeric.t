#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha rgb16 icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0156_process_alpha_rgb16_icc_numeric 1>&2 || {
    echo "not ok 1 - process alpha rgb16 icc numeric"
    exit 0
}
echo "ok 1 - process alpha rgb16 icc numeric"
exit 0
