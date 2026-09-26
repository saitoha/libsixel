#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify builtin lut physical pcs.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0020_builtin_lut_physical_pcs" || {
    echo "not ok 1 - builtin lut physical pcs"
    exit 0
}
echo "ok 1 - builtin lut physical pcs"
exit 0
