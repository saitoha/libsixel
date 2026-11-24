#!/bin/sh
# A full queue has a specific diagnostic.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" diagnostics/status/0002_status_queue_full || {
    printf '\n%s\n' "not ok 1 - a full queue has a specific diagnostic"
    exit 0
}
printf '\n%s\n' "ok 1 - a full queue has a specific diagnostic"
exit 0
