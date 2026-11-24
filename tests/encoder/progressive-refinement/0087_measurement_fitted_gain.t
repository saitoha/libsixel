#!/bin/sh
# Verify measurement fitted gain.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0119_measurement_fitted_gain" || {
    printf '\n%s\n' "not ok 1 - measurement fitted gain"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement fitted gain"
exit 0
