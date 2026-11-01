#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify lab alpha truncated reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0255_lab_alpha_truncated_reject 1>&2 || {
    echo "not ok 1 - lab alpha truncated reject"
    exit 0
}
echo "ok 1 - lab alpha truncated reject"
exit 0
