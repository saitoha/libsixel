#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify process alpha rgb32 extended numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0130_process_alpha_rgb32_extended_numeric 1>&2 || {
    echo "not ok 1 - process alpha rgb32 extended numeric"
    exit 0
}
echo "ok 1 - process alpha rgb32 extended numeric"
exit 0
