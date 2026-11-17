#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify absolute unchanged against an algebraic reference.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bpc/0008_absolute_unchanged || {
    echo 'not ok 1 - absolute unchanged'
    exit 0
}
echo 'ok 1 - absolute unchanged'
exit 0
