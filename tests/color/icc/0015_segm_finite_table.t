#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify segm finite table.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0015_segm_finite_table || {
    printf '\n%s\n' "not ok 1 - segm finite table"
    exit 0
}
printf '\n%s\n' "ok 1 - segm finite table"
exit 0
