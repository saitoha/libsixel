#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify builtin inplace expansion.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0021_builtin_inplace_expansion" || {
    echo "not ok 1 - builtin inplace expansion"
    exit 0
}
echo "ok 1 - builtin inplace expansion"
exit 0
