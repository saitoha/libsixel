#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    ycbcr422 unsupported || {
    printf '\n%s\n' 'not ok 1 - Storage-only packed frames reject conversion and encoding'
    exit 0
}
printf '\n%s\n' 'ok 1 - Storage-only packed frames reject conversion and encoding'
exit 0
