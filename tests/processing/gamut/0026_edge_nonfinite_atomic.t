#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0026_edge_nonfinite_atomic || {
    echo "not ok 1 - edge nonfinite atomic"
    exit 0
}
echo "ok 1 - edge nonfinite atomic"
exit 0
