#!/bin/sh
# Verify sampling resolution occurs before scheduler allocation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0026_sampling_auto_resolution" || {
    echo "not ok 1 - automatic sampling resolution"
    exit 0
}

echo "ok 1 - automatic sampling resolution"
exit 0
