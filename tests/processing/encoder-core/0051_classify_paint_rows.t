#!/bin/sh
# Verify classify paint rows.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0051_classify_paint_rows" || {
    echo "not ok 1 - classify paint rows"
    exit 0
}

echo "ok 1 - classify paint rows"
exit 0
