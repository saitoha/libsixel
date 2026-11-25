#!/bin/sh
# A count declared before the palette retains its allocation size.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical count-first || {
    printf '\n%s\n' 'not ok 1 - A count declared before the palette retains its allocation size'
    exit 0
}
printf '\n%s\n' 'ok 1 - A count declared before the palette retains its allocation size'
exit 0
