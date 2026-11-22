#!/bin/sh
# Check that packed float promotion preserves indices on allocation failure.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical packed-retry || {
    echo 'not ok 1 - packed float promotion preserves indices on allocation failure'
    exit 0
}
echo 'ok 1 - packed float promotion preserves indices on allocation failure'
exit 0
