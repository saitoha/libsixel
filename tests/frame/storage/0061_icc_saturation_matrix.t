#!/bin/sh
# A2B2 retains the relative matrix transform.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical saturation-matrix || {
    printf '\n%s\n' 'not ok 1 - A2B2 retains the relative matrix transform'
    exit 0
}
printf '\n%s\n' 'ok 1 - A2B2 retains the relative matrix transform'
exit 0
