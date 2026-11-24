#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier arith multiblock prog.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0164_jp_hier_arith_multiblock_prog || {
    printf '\n%s\n' "not ok 1 - hier arith multiblock prog"
    exit 0
}
printf '\n%s\n' "ok 1 - hier arith multiblock prog"
exit 0
