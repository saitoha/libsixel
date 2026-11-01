#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk alpha negative reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0231_cmyk_alpha_negative_reject 1>&2 || {
    echo "not ok 1 - cmyk alpha negative reject"
    exit 0
}
echo "ok 1 - cmyk alpha negative reject"
exit 0
