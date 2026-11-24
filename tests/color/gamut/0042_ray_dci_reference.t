#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0042_ray_dci_reference || {
    printf '\n%s\n' "not ok 1 - edge dci reference"
    exit 0
}
printf '\n%s\n' "ok 1 - edge dci reference"
exit 0
