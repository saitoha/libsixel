#!/bin/sh
# TAP test: Bandit prune stratified unique samples keep deterministic output.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    bandit-prune-stratified-unique-sample-reproducibility \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids bandit stratified sample reproducibility failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids bandit stratified sample reproducibility passed"
exit 0
