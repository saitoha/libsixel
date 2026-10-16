#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify filter sample parallel exact.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0046_filter_sample_parallel_exact" || {
    echo "not ok 1 - filter sample parallel exact"
    exit 0
}
echo "ok 1 - filter sample parallel exact"
exit 0
