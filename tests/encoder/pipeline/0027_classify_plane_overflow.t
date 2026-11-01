#!/bin/sh
# Verify classify plane overflow.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0055_classify_plane_overflow" || {
    echo "not ok 1 - classify plane overflow"
    exit 0
}

echo "ok 1 - classify plane overflow"
exit 0
