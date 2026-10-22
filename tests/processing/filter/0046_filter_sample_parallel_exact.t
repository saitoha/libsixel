#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Compare ordered samples, masks and encoded bytes for RGB888.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0046_filter_sample_parallel_exact" rgb888 || {
    echo "not ok 1 - RGB888 sample parallel exact"
    exit 0
}
echo "ok 1 - RGB888 sample parallel exact"
exit 0
