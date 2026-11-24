#!/bin/sh
# Policy: docs/loader/builtin-cms.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0021_absolute_matrix_xyz || {
    printf '\n%s\n' "not ok 1 - absolute matrix conversion uses media white once"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute matrix conversion uses media white once"
exit 0
