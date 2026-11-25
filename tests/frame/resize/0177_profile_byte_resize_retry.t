#!/bin/sh
# Verify that failed byte resize retains the original profiled frame.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical byte-resize-retry || {
    printf '\n%s\n' 'not ok 1 - failed byte resize retains the original profiled frame'
    exit 0
}
printf '\n%s\n' 'ok 1 - failed byte resize retains the original profiled frame'
exit 0
