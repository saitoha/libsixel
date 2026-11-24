#!/bin/sh
# Verify reject reverse-only ICC and retry with a forward ICC.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    public-icc-direction >&2 || {
    echo 'not ok 1 - reject reverse-only ICC and retry with a forward ICC'
    exit 0
}
echo 'ok 1 - reject reverse-only ICC and retry with a forward ICC'
exit 0
