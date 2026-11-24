#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify para finite table.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0014_para_finite_table || {
    printf '\n%s\n' "not ok 1 - para finite table"
    exit 0
}
printf '\n%s\n' "ok 1 - para finite table"
exit 0
