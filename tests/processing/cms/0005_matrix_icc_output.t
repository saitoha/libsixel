#!/bin/sh
# Verify matrix icc output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0005_matrix_icc_output" || {
    echo "not ok 1 - matrix icc output"
    exit 0
}

echo "ok 1 - matrix icc output"
exit 0
