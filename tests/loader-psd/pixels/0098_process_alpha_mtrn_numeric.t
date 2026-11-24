#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha mtrn numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0107_process_alpha_mtrn_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - process alpha mtrn numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - process alpha mtrn numeric"
exit 0
