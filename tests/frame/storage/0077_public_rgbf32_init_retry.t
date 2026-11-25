#!/bin/sh
# Reject invalid dimensions without losing the borrowed pixels or color model.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical \
    public-rgbf32-retry || {
    printf '\n%s\n' 'not ok 1 - explicit RGB float initialization preserves state for retry'
    exit 0
}
printf '\n%s\n' 'ok 1 - explicit RGB float initialization preserves state for retry'
exit 0
