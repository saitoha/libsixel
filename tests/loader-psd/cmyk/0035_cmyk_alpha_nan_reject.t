#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk alpha nan reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0224_cmyk_alpha_nan_reject 1>&2 || {
    echo "not ok 1 - cmyk alpha nan reject"
    exit 0
}
echo "ok 1 - cmyk alpha nan reject"
exit 0
