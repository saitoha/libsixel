#!/bin/sh
# Policy: docs/loader/builtin-cms.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0021_absolute_matrix_xyz || {
    echo "not ok 1 - absolute matrix conversion uses media white once"
    exit 0
}

echo "ok 1 - absolute matrix conversion uses media white once"
exit 0
