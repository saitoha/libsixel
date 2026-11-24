#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify builtin byte to float.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0017_builtin_byte_to_float" || {
    printf '\n%s\n' "not ok 1 - builtin byte to float"
    exit 0
}
printf '\n%s\n' "ok 1 - builtin byte to float"
exit 0
