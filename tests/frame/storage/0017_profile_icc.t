#!/bin/sh
# Check that the ICC frame accounts for its profile and retains its red primary.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical icc || {
    printf '\n%s\n' 'not ok 1 - ICC frame accounts for its profile and retains its red primary'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC frame accounts for its profile and retains its red primary'
exit 0
