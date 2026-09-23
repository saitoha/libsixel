#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify display kind reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0031_display_kind_reject 1>&2 || {
    echo "not ok 1 - display kind reject"
    exit 0
}
echo "ok 1 - display kind reject"
exit 0
