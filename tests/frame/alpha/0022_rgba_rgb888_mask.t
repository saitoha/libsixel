#!/bin/sh
# RGB888 conversion preserves zero alpha in the decoded paint mask.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical rgba-byte-mask || {
    printf '\n%s\n' 'not ok 1 - RGB888 conversion preserves zero alpha in the decoded paint mask'
    exit 0
}
printf '\n%s\n' 'ok 1 - RGB888 conversion preserves zero alpha in the decoded paint mask'
exit 0
