#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify builtin extended transfer.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0016_builtin_extended_transfer" || {
    printf '\n%s\n' "not ok 1 - builtin extended transfer"
    exit 0
}
printf '\n%s\n' "ok 1 - builtin extended transfer"
exit 0
