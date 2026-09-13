#!/bin/sh
# Verify extended lab inverse against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0009_extended_lab_inverse" || {
    echo "not ok 1 - extended lab inverse"
    exit 0
}

echo "ok 1 - extended lab inverse"
exit 0
