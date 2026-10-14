#!/bin/sh
# Verify ladder finite noise transition.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0139_ladder_finite_noise_transition" || {
    echo "not ok 1 - ladder finite noise transition"
    exit 0
}
echo "ok 1 - ladder finite noise transition"
exit 0
