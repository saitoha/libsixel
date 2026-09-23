#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 restart16 against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0181_sof3_restart16_numeric 1>&2 || {
    echo "not ok 1 - sof3_restart16 numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_restart16 numeric reconstruction"
exit 0
