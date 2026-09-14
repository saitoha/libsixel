#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify builtin byte to float.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0017_builtin_byte_to_float" || {
    echo "not ok 1 - builtin byte to float"
    exit 0
}
echo "ok 1 - builtin byte to float"
exit 0
