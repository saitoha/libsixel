#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# Verify builtin APNG gamma transfer.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "apng/0001_float" 13 || {
    printf '\n%s\n' "not ok 1 - builtin APNG gamma transfer"
    exit 0
}
printf '\n%s\n' "ok 1 - builtin APNG gamma transfer"
exit 0
