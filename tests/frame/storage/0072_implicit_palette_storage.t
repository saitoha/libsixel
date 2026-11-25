#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical implicit-palette-storage || {
    printf '\n%s\n' 'not ok 1 - Reshaping retains the inferred palette extent'
    exit 0
}
printf '\n%s\n' 'ok 1 - Reshaping retains the inferred palette extent'
exit 0
