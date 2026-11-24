#!/bin/sh
# pal8 clip full t8 samples equal the serial stream.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    filter/0046_filter_sample_parallel_exact 1 1 2 0 8 || {
    printf '\n%s\n' "not ok 1 - pal8 clip full t8 samples equal the serial stream"
    exit 0
}
printf '\n%s\n' "ok 1 - pal8 clip full t8 samples equal the serial stream"
exit 0
