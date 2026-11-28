#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422 clone || {
    printf '\n%s\n' 'not ok 1 - Packed clones own only active row bytes and retain metadata'
    exit 0
}
printf '\n%s\n' 'ok 1 - Packed clones own only active row bytes and retain metadata'
exit 0
