#!/bin/sh
# Policy: docs/loader/builtin-cms.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0022_absolute_lut_xyz || {
    printf '\n%s\n' "not ok 1 - absolute LUT conversion uses both media whites"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute LUT conversion uses both media whites"
exit 0
