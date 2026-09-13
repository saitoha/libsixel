#!/bin/sh
# Verify perturbed FS error propagation after early 6delta keep.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0017_perturb_keep_early" || {
    echo "not ok 1 - perturbed early keep"
    exit 0
}
echo "ok 1 - perturbed early keep"
exit 0
