#!/bin/sh
# Run the parallel private dense-cache regression test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "lookup/0013_parallel_private_dense_cache" || {
    echo "not ok 1 - parallel private dense lookup cache"
    exit 0
}

echo "ok 1 - parallel private dense lookup cache"
exit 0
