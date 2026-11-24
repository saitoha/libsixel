#!/bin/sh
# Gate the direction-dependent gain spread of the separable scaler.
# Test-plan: docs/testing/resampling-coverage.md
set -eux

echo "1..1"
set -v

baseline="${TOP_SRCDIR}/tests/data/expected/resampling-measurement-baselines.csv"
measurement="${TOP_SRCDIR}/docs/encoder/preprocessing/resampling/measurements/resampling-isotropy.csv"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    geometry/resampling_measurement_baseline \
    isotropy "${baseline}" "${measurement}" || {
    printf '\n%s\n' "not ok 1 - isotropy numeric baseline changed"
    exit 0
}

printf '\n%s\n' "ok 1 - isotropy numeric baseline is within tolerance"
exit 0
