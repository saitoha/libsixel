#!/bin/sh
# Gate near-cutoff retention and above-Nyquist alias leakage.
# Test-plan: docs/testing/resampling-coverage.md
set -eux

echo "1..1"
set -v

baseline="${TOP_SRCDIR}/tests/data/expected/resampling-measurement-baselines.csv"
measurement="${TOP_SRCDIR}/docs/functionality/resampling/measurements/resampling-frequency-response.csv"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_measurement_baseline \
    frequency "${baseline}" "${measurement}" || {
    echo "not ok" 1 - "frequency-response numeric baseline changed"
    exit 0
}

echo "ok" 1 - "frequency-response numeric baseline is within tolerance"
exit 0
