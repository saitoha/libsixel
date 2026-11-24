#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify arith wide seq with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0160_jp_arith_wide_seq || {
    printf '\n%s\n' "not ok 1 - arith wide seq"
    exit 0
}
printf '\n%s\n' "ok 1 - arith wide seq"
exit 0
