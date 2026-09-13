#!/bin/sh
# Verify perturbed FS error propagation after lookup 6delta keep.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0018_perturb_keep_lookup" || {
    echo "not ok 1 - perturbed lookup keep"
    exit 0
}
echo "ok 1 - perturbed lookup keep"
exit 0
