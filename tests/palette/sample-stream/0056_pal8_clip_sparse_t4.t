#!/bin/sh
# pal8 clip sparse t4 samples equal the serial stream.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    filter/0046_filter_sample_parallel_exact 1 1 3 0 4 || {
    echo "not ok 1 - pal8 clip sparse t4 samples equal the serial stream"
    exit 0
}
echo "ok 1 - pal8 clip sparse t4 samples equal the serial stream"
exit 0
