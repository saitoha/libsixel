#!/bin/sh
# Gate decoded quality and encoded size with reviewable numeric bands.
# Test-plan: docs/testing/resampling-coverage.md
set -eux

echo "1..1"
set -v

baseline="${TOP_SRCDIR}/tests/data/expected/resampling-measurement-baselines.csv"
measurement="${TOP_SRCDIR}/docs/functionality/resampling/measurements/resampling-quality-size.csv"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_measurement_baseline \
    quality "${baseline}" "${measurement}" || {
    echo "not ok" 1 - "quality or encoded-size numeric baseline changed"
    exit 0
}

echo "ok" 1 - "quality and encoded-size baselines are within tolerance"
exit 0
