#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify timeline parallel sample.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "timeline/0008_timeline_parallel_sample" || {
    echo "not ok 1 - timeline parallel sample"
    exit 0
}
echo "ok 1 - timeline parallel sample"
exit 0
