#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0086_mpet_legacy_color || {
    printf '\n%s\n' "not ok 1 - Legacy LUT color survives unused and failing mpet tags"
    exit 0
}

printf '\n%s\n' "ok 1 - Legacy LUT color survives unused and failing mpet tags"
exit 0
