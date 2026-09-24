#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray byte control.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0176_gray_byte_control 1>&2 || {
    echo "not ok 1 - gray byte control"
    exit 0
}
echo "ok 1 - gray byte control"
exit 0
