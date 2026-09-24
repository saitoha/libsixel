#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray alternate numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0167_gray_alternate_numeric 1>&2 || {
    echo "not ok 1 - gray alternate numeric"
    exit 0
}
echo "ok 1 - gray alternate numeric"
exit 0
