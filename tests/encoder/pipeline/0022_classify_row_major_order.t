#!/bin/sh
# Verify classify row major order.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0050_classify_row_major_order" || {
    echo "not ok 1 - classify row major order"
    exit 0
}

echo "ok 1 - classify row major order"
exit 0
