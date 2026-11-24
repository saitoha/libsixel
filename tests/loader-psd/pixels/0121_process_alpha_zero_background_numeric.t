#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha zero background numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0157_process_alpha_zero_background_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - process alpha zero background numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - process alpha zero background numeric"
exit 0
