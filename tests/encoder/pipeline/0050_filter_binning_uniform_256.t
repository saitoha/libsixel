#!/bin/sh
# Verify the half-open uniform hard grid preserves legacy K-center bins.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0041_filter_binning_uniform_256" || {
    printf '\n%s\n' "not ok 1 - uniform-256 hard-grid contract"
    exit 0
}

printf '\n%s\n' "ok 1 - uniform-256 hard-grid contract"
exit 0
