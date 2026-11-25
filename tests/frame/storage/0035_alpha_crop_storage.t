#!/bin/sh
# A cropped frame retains its straight alpha allocation until replacement.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical crop-alpha-storage || {
    printf '\n%s\n' 'not ok 1 - A cropped frame retains its straight alpha allocation until replacement'
    exit 0
}
printf '\n%s\n' 'ok 1 - A cropped frame retains its straight alpha allocation until replacement'
exit 0
