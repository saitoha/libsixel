#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate empty numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0062_alternate_empty_numeric 1>&2 || {
    echo "not ok 1 - alternate empty numeric"
    exit 0
}
echo "ok 1 - alternate empty numeric"
exit 0
