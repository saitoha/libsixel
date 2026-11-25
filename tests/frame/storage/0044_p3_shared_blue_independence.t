#!/bin/sh
# Extended Display P3 blue cannot change encoded red or green.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical p3-blue || {
    printf '\n%s\n' 'not ok 1 - Extended Display P3 blue cannot change encoded red or green'
    exit 0
}
printf '\n%s\n' 'ok 1 - Extended Display P3 blue cannot change encoded red or green'
exit 0
