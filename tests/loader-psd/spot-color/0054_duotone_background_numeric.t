#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone background numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0010_duotone_background_numeric 1>&2 || {
    echo "not ok 1 - duotone background numeric"
    exit 0
}
echo "ok 1 - duotone background numeric"
exit 0
