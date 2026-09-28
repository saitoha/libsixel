#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify builtin APNG byte fastpath.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "apng/0001_float" 7 || {
    echo "not ok 1 - builtin APNG byte fastpath"
    exit 0
}
echo "ok 1 - builtin APNG byte fastpath"
exit 0
