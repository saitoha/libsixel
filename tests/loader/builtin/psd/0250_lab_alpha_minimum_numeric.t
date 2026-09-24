#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab alpha minimum numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0250_lab_alpha_minimum_numeric 1>&2 || {
    echo "not ok 1 - lab alpha minimum numeric"
    exit 0
}
echo "ok 1 - lab alpha minimum numeric"
exit 0
