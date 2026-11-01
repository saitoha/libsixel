#!/bin/sh
# float32 clip high t2 samples equal the serial stream.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    filter/0046_filter_sample_parallel_exact 2 1 1 0 2 || {
    echo "not ok 1 - float32 clip high t2 samples equal the serial stream"
    exit 0
}
echo "ok 1 - float32 clip high t2 samples equal the serial stream"
exit 0
