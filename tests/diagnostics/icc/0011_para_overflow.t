#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify para overflow.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0011_para_overflow || {
    echo "not ok 1 - para overflow"
    exit 0
}
echo "ok 1 - para overflow"
exit 0
