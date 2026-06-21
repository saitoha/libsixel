#!/bin/sh
# Verify OR mode direct-color decode uses the composed palette index.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0004_decoder_ormode_direct_overlay" || {
    echo "not ok 1 - 0004_decoder_ormode_direct_overlay"
    exit 0
}

echo "ok 1 - 0004_decoder_ormode_direct_overlay"
exit 0
