#!/bin/sh
# Two shared RGB primary axes preserve the remaining channel exactly.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical shared-red-blue || {
    printf '\n%s\n' 'not ok 1 - Two shared RGB primary axes preserve the remaining channel exactly'
    exit 0
}
printf '\n%s\n' 'ok 1 - Two shared RGB primary axes preserve the remaining channel exactly'
exit 0
