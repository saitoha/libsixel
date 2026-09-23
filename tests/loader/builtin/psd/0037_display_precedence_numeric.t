#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify display precedence numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0037_display_precedence_numeric 1>&2 || {
    echo "not ok 1 - display precedence numeric"
    exit 0
}
echo "ok 1 - display precedence numeric"
exit 0
