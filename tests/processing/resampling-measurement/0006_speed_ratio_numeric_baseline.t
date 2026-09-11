#!/bin/sh
# Gate relative method cost without imposing an absolute host timing.
# Test-plan: docs/testing/resampling-coverage.md
set -eux

echo "1..1"
set -v

baseline="${TOP_SRCDIR}/tests/data/expected/resampling-measurement-baselines.csv"
measurement="${TOP_SRCDIR}/docs/functionality/resampling/measurements/resampling-speed.csv"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_measurement_baseline \
    speed "${baseline}" "${measurement}" || {
    echo "not ok" 1 - "relative-speed numeric baseline changed"
    exit 0
}

echo "ok" 1 - "relative-speed baseline is within tolerance"
exit 0
