#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify segm loader recovery.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0017_segm_loader_recovery || {
    echo "not ok 1 - segm loader recovery"
    exit 0
}
echo "ok 1 - segm loader recovery"
exit 0
