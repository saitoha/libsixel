#!/bin/sh

# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0050_loader_osc11_colorspec_parser" || {
    echo "not ok 1 - OSC 11 colorspec accepted forms"
    exit 0
}

echo "ok 1 - OSC 11 colorspec accepted forms"
exit 0
