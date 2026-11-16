#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0090_mpet_raw_lab_target || {
    echo "not ok 1 - An ordinary Lab target can consume mpet XYZ without a reverse LUT"
    exit 0
}

echo "ok 1 - An ordinary Lab target can consume mpet XYZ without a reverse LUT"
exit 0
