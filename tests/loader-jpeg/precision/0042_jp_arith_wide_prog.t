#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify arith wide prog with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0161_jp_arith_wide_prog || {
    printf '\n%s\n' "not ok 1 - arith wide prog"
    exit 0
}
printf '\n%s\n' "ok 1 - arith wide prog"
exit 0
