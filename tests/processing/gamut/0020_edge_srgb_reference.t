#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0020_edge_srgb_reference || {
    echo "not ok 1 - edge srgb reference"
    exit 0
}
echo "ok 1 - edge srgb reference"
exit 0
