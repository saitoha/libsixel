#!/bin/sh
# Verify classify mask fillable.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0052_classify_mask_fillable" || {
    echo "not ok 1 - classify mask fillable"
    exit 0
}

echo "ok 1 - classify mask fillable"
exit 0
