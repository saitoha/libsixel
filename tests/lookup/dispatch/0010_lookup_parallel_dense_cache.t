#!/bin/sh
# Run the parallel dense-cache regression test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "lookup/0013_parallel_dense_cache" || {
    printf '\n%s\n' "not ok 1 - parallel dense lookup cache"
    exit 0
}

printf '\n%s\n' "ok 1 - parallel dense lookup cache"
exit 0
