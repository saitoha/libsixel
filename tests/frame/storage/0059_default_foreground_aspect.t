#!/bin/sh
# Aspect changes preserve the default foreground palette entry.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical default-aspect || {
    printf '\n%s\n' 'not ok 1 - Aspect changes preserve the default foreground palette entry'
    exit 0
}
printf '\n%s\n' 'ok 1 - Aspect changes preserve the default foreground palette entry'
exit 0
