#!/bin/sh
# pal8 clip stride one t1 output equals serial extraction.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    filter/0046_filter_sample_parallel_exact 1 1 4 1 1 || {
    echo "not ok 1 - pal8 clip stride one t1 output equals serial extraction"
    exit 0
}
echo "ok 1 - pal8 clip stride one t1 output equals serial extraction"
exit 0
