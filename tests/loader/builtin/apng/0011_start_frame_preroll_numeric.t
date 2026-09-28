#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# Verify builtin APNG start frame preroll.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "apng/0001_float" 10 || {
    echo "not ok 1 - builtin APNG start frame preroll"
    exit 0
}
echo "ok 1 - builtin APNG start frame preroll"
exit 0
