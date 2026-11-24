#!/bin/sh
# Reject slices of frame-owned pixels and preserve the frame for retry.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    public-rgbf32-owned-slice || {
    echo 'not ok 1 - RGB float initialization rejects frame-owned slices'
    exit 0
}
echo 'ok 1 - RGB float initialization rejects frame-owned slices'
exit 0
