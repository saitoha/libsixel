#!/bin/sh
# Verify prepare history invalidation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0065_prepare_history_invalidation" || {
    echo "not ok 1 - prepare history invalidation"
    exit 0
}
echo "ok 1 - prepare history invalidation"
exit 0
