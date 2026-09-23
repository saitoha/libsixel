#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify display opacity reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0030_display_opacity_reject 1>&2 || {
    echo "not ok 1 - display opacity reject"
    exit 0
}
echo "ok 1 - display opacity reject"
exit 0
