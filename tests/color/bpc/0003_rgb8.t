#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify rgb8 against an algebraic reference.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bpc/0003_rgb8 || {
    echo 'not ok 1 - rgb8'
    exit 0
}
echo 'ok 1 - rgb8'
exit 0
