#!/bin/sh
# Verify prepare source invalidation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0064_prepare_source_invalidation" || {
    echo "not ok 1 - prepare source invalidation"
    exit 0
}
echo "ok 1 - prepare source invalidation"
exit 0
