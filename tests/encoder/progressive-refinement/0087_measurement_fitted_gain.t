#!/bin/sh
# Verify measurement fitted gain.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0119_measurement_fitted_gain" || {
    echo "not ok 1 - measurement fitted gain"
    exit 0
}
echo "ok 1 - measurement fitted gain"
exit 0
