#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0086_mpet_legacy_color || {
    echo "not ok 1 - Legacy LUT color survives unused and failing mpet tags"
    exit 0
}

echo "ok 1 - Legacy LUT color survives unused and failing mpet tags"
exit 0
