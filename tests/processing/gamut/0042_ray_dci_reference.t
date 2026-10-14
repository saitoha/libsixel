#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0042_ray_dci_reference || {
    echo "not ok 1 - edge dci reference"
    exit 0
}
echo "ok 1 - edge dci reference"
exit 0
