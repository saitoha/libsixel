#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone 16 numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0006_duotone_16_numeric 1>&2 || {
    echo "not ok 1 - duotone 16 numeric"
    exit 0
}
echo "ok 1 - duotone 16 numeric"
exit 0
