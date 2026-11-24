#!/bin/sh
# Gate near-cutoff retention and above-Nyquist alias leakage.
# Test-plan: docs/testing/resampling-coverage.md
set -eux

echo "1..1"
set -v

baseline="${TOP_SRCDIR}/tests/data/expected/resampling-measurement-baselines.csv"
measurement="${TOP_SRCDIR}/docs/encoder/preprocessing/resampling/measurements/resampling-frequency-response.csv"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_measurement_baseline \
    frequency "${baseline}" "${measurement}" || {
    printf '\n%s\n' "not ok 1 - frequency-response numeric baseline changed"
    exit 0
}

printf '\n%s\n' "ok 1 - frequency-response numeric baseline is within tolerance"
exit 0
