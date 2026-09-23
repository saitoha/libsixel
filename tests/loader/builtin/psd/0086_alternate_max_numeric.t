#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate max numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0086_alternate_max_numeric 1>&2 || {
    echo "not ok 1 - alternate max numeric"
    exit 0
}
echo "ok 1 - alternate max numeric"
exit 0
