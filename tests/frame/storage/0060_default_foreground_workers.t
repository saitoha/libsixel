#!/bin/sh
# Parallel indexed decoding reports its painted foreground register.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical default-workers || {
    echo 'not ok 1 - Parallel indexed decoding reports its painted foreground register'
    exit 0
}
echo 'ok 1 - Parallel indexed decoding reports its painted foreground register'
exit 0
