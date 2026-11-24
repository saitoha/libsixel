#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify segm overflow.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0013_segm_overflow || {
    printf '\n%s\n' "not ok 1 - segm overflow"
    exit 0
}
printf '\n%s\n' "ok 1 - segm overflow"
exit 0
