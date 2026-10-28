#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0063_pipeline_bits || {
    echo "not ok 1 - pipeline bits"
    exit 0
}
echo "ok 1 - pipeline bits"
exit 0
