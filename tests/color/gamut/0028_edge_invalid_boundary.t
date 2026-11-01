#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0028_edge_invalid_boundary || {
    echo "not ok 1 - edge invalid boundary"
    exit 0
}
echo "ok 1 - edge invalid boundary"
exit 0
