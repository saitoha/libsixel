#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify para loader recovery.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0016_para_loader_recovery || {
    printf '\n%s\n' "not ok 1 - para loader recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - para loader recovery"
exit 0
