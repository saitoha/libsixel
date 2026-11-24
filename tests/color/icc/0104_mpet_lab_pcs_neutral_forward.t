#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0104_mpet_lab_pcs_neutral_forward || {
    printf '\n%s\n' "not ok 1 - Lab PCS to XYZ retains near-black chroma"
    exit 0
}

printf '\n%s\n' "ok 1 - Lab PCS to XYZ retains near-black chroma"
exit 0
