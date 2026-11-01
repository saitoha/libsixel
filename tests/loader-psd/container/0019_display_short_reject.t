#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify display short reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0029_display_short_reject 1>&2 || {
    echo "not ok 1 - display short reject"
    exit 0
}
echo "ok 1 - display short reject"
exit 0
