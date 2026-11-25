#!/bin/sh
# Check that the profile conversion preserves input and definition for retry.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical retry || {
    printf '\n%s\n' 'not ok 1 - profile conversion preserves input and definition for retry'
    exit 0
}
printf '\n%s\n' 'ok 1 - profile conversion preserves input and definition for retry'
exit 0
