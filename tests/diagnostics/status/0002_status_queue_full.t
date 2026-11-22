#!/bin/sh
# A full queue has a specific diagnostic.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" diagnostics/status/0002_status_queue_full || {
    echo "not ok 1 - a full queue has a specific diagnostic"
    exit 0
}
echo "ok 1 - a full queue has a specific diagnostic"
exit 0
