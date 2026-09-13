#!/bin/sh
# Verify parametric type4.
# Policy: docs/loader/builtin-cms.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "icc/0009_parametric_type4" || {
    echo "not ok 1 - parametric type4"
    exit 0
}

echo "ok 1 - parametric type4"
exit 0
