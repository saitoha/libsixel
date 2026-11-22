#!/bin/sh
# Check that the profile conversion preserves input and definition for retry.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical retry || {
    echo 'not ok 1 - profile conversion preserves input and definition for retry'
    exit 0
}
echo 'ok 1 - profile conversion preserves input and definition for retry'
exit 0
