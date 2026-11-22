#!/bin/sh
# Frame storage includes the shared ICC payload once and excludes released parser allocations.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical profile-storage || {
    echo 'not ok 1 - frame storage counts only retained ICC allocations'
    exit 0
}
echo 'ok 1 - frame storage counts only retained ICC allocations'
exit 0
