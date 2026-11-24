#!/bin/sh
# Policy: docs/loader/builtin-cms.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0023_absolute_definition_xyz || {
    printf '\n%s\n' "not ok 1 - absolute XYZ definition applies and reverses media white"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute XYZ definition applies and reverses media white"
exit 0
