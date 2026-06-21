#!/bin/sh
# Verify OR mode overlays bit-plane selectors into final palette indexes.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0002_decoder_ormode_overlay" || {
    echo "not ok 1 - 0002_decoder_ormode_overlay"
    exit 0
}

echo "ok 1 - 0002_decoder_ormode_overlay"
exit 0
