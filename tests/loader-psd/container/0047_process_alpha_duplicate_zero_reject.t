#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha duplicate zero reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0143_process_alpha_duplicate_zero_reject 1>&2 || {
    echo "not ok 1 - process alpha duplicate zero reject"
    exit 0
}
echo "ok 1 - process alpha duplicate zero reject"
exit 0
