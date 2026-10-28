#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0068_filter_dag || {
    echo "not ok 1 - filter dag"
    exit 0
}
echo "ok 1 - filter dag"
exit 0
