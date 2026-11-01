#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab32 alpha reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0254_lab32_alpha_reject 1>&2 || {
    echo "not ok 1 - lab32 alpha reject"
    exit 0
}
echo "ok 1 - lab32 alpha reject"
exit 0
