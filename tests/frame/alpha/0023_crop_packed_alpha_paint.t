#!/bin/sh
# Cropping packed alpha preserves zero coverage in the decoded image.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical crop-packed-alpha || {
    printf '\n%s\n' 'not ok 1 - Cropping packed alpha preserves zero coverage in the decoded image'
    exit 0
}
printf '\n%s\n' 'ok 1 - Cropping packed alpha preserves zero coverage in the decoded image'
exit 0
