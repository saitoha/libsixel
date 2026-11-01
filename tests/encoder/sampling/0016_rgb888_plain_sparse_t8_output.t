#!/bin/sh
# rgb888 plain sparse t8 output equals serial extraction.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    filter/0046_filter_sample_parallel_exact 0 0 3 1 8 || {
    echo "not ok 1 - rgb888 plain sparse t8 output equals serial extraction"
    exit 0
}
echo "ok 1 - rgb888 plain sparse t8 output equals serial extraction"
exit 0
