#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify arith wide seq with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0160_jp_arith_wide_seq || {
    echo "not ok 1 - arith wide seq"
    exit 0
}
echo "ok 1 - arith wide seq"
exit 0
