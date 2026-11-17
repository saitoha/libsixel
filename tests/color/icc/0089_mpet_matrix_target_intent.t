#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0089_mpet_matrix_target_intent || {
    echo "not ok 1 - An invertible matrix target preserves relative intent precedence"
    exit 0
}

echo "ok 1 - An invertible matrix target preserves relative intent precedence"
exit 0
