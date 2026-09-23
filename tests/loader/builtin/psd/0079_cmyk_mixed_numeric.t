#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk mixed numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0079_cmyk_mixed_numeric 1>&2 || {
    echo "not ok 1 - cmyk mixed numeric"
    exit 0
}
echo "ok 1 - cmyk mixed numeric"
exit 0
