#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier arith multiblock seq.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0163_jp_hier_arith_multiblock_seq || {
    printf '\n%s\n' "not ok 1 - hier arith multiblock seq"
    exit 0
}
printf '\n%s\n' "ok 1 - hier arith multiblock seq"
exit 0
