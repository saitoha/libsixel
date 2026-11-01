#!/bin/sh
# Verify classify source overflow.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0054_classify_source_overflow" || {
    echo "not ok 1 - classify source overflow"
    exit 0
}

echo "ok 1 - classify source overflow"
exit 0
