#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab alpha icc malformed reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0258_lab_alpha_icc_malformed_reject 1>&2 || {
    echo "not ok 1 - lab alpha icc malformed reject"
    exit 0
}
echo "ok 1 - lab alpha icc malformed reject"
exit 0
