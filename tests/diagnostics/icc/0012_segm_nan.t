#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify segm nan.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0012_segm_nan || {
    echo "not ok 1 - segm nan"
    exit 0
}
echo "ok 1 - segm nan"
exit 0
