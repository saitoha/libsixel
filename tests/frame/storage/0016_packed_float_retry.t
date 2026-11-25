#!/bin/sh
# Check that packed float promotion preserves indices on allocation failure.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical packed-retry || {
    printf '\n%s\n' 'not ok 1 - packed float promotion preserves indices on allocation failure'
    exit 0
}
printf '\n%s\n' 'ok 1 - packed float promotion preserves indices on allocation failure'
exit 0
